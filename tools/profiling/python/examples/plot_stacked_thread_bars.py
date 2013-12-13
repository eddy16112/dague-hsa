#!/usr/bin/env python

from __future__ import print_function
import os, sys
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import mpl_prefs
import binprof_utils as p3_utils
import pandas as pd

bar_width = 0.8

def longest_substr(data):
    substr = ''
    if len(data) > 1 and len(data[0]) > 0:
        for i in range(len(data[0])):
            for j in range(len(data[0])-i+1):
                if j > len(substr) and is_substr(data[0][i:i+j], data):
                    substr = data[0][i:i+j]
    return substr

def is_substr(find, data):
    if len(data) < 1 and len(find) < 1:
        return False
    for i in range(len(data)):
        if find not in data[i]:
            return False
    return True

def rreplace(s, old, new, count = 1):
    return (s[::-1].replace(old[::-1], new[::-1], count))[::-1]

patches = []
patch_names = []
for name, color in mpl_prefs.task_slice_colors.iteritems():
    patches.append(matplotlib.patches.Patch(facecolor=color))
    patch_names.append(name)

if __name__ == '__main__':
    min_trials = sys.maxint
    filenames = []

    for index, arg in enumerate(sys.argv[1:]):
        if os.path.exists(arg):
            filenames.append(arg)

    processed_filename_groups = p3_utils.preprocess_profiles(filenames, convert=False)

    for fname_group in processed_filename_groups:
        profile = p3_utils.autoload_profiles(fname_group)[0]

        figure = plt.figure()
        ax = figure.add_subplot(111)

        exec_event = None
        select_event = None
        compl_event = None
        prep_event = None

        for event_name in profile.event_types.keys():
            if event_name.startswith('PAPI_CORE_EXEC'):
                exec_event = profile.event_types[event_name]
            elif 'CORE_SEL' in event_name:
                select_event = profile.event_types[event_name]
            elif 'CORE_COMPL' in event_name:
                compl_event = profile.event_types[event_name]
            elif 'CORE_PREP' in event_name:
                prep_event = profile.event_types[event_name]

        # this pandas builtin allows a one-time grouping of the events
        # instead of a new iteration over all the events per thread
        thread_group_events = profile.events.groupby(['thread_id', 'type'])
        # but since the groups may be out-of-order, we need to prepare to
        # collect the information, then sum it, and then plot it in a separate loop
        # after sorting the sums:

        # PREPARE
        thread_bars = dict()
        for th_id in profile.threads['id']:
            thread_bars[th_id] = dict()
            thread_bars[th_id]['thread_duration'] = profile.threads['duration'][
                profile.threads['id'] == th_id]

        # COLLECT AND SUM
        for (th_id, event_type), group in thread_group_events:
            total = group['duration'].sum()
            if select_event == event_type:
                selection_time = group['selection_time'].sum()
                if total >= selection_time:
                    thread_bars[th_id]['Starvation'] = total - selection_time
                else:
                    thread_bars[th_id]['Starvation'] = 0
                thread_bars[th_id]['Selection'] = selection_time
            elif exec_event == event_type:
                thread_bars[th_id]['Execution'] = total
            elif compl_event == event_type:
                thread_bars[th_id]['Completion'] = total
            elif prep_event ==  event_type:
                thread_bars[th_id]['Preparation'] = total

        # PLOT
        for th_id, bar_data in thread_bars.iteritems():
            bottom_sum = 0.0
            for key, bar_color in mpl_prefs.task_slice_colors.iteritems():
                try: # the event type may not be present in the bar data
                    # print('key {} th_id {} val {}'.format(key, th_id, bar_data[key]))
                    ax.bar(th_id + 0.1, bar_data[key], color=bar_color,
                           width=bar_width, bottom=bottom_sum, linewidth=0)
                    bottom_sum += bar_data[key]
                except KeyError as e:
                    pass # but if it wasn't present, no problem
            # now also plot the framework value (the rest)
            th_duration = bar_data['thread_duration'].sum()
            framework_time = th_duration - bottom_sum
            if framework_time < 0:
                print('Warning: your profile may have inaccurate data, ' +
                      'as the total time of all component events for ' +
                      'thread {} ({}) is greater than the recorded '.format(th_id, bottom_sum) +
                      'duration of that thread ({}).'.format(th_duration))
            else:
                ax.bar(th_id + 0.1, framework_time,
                       bottom=bottom_sum, width=bar_width, linewidth=0,
                       color=mpl_prefs.task_slice_colors['Framework'])

        ax.grid(True)
        # ax.ticklabel_format(style='sci', scilimits=(0,9), axis='y')
        ax.set_xticks(np.arange(0.5, 0.5 + len(profile.threads), 4))
        ax.set_xticklabels([str(i * 4) for i in range(len(profile.threads))], rotation=25.0)
        ax.legend(patches, patch_names, loc='center', ncol=3, fancybox=True, shadow=True)
        ax.set_xlabel('thread id')
        ax.set_ylabel('runtime in nanoseconds')
        ax.set_title(
            'Aggregated task lifecycle components for single trial on {}\n'.format(
                profile.hostname) +
            'N={}, NB={}, sched {}, {} gflops/s, {} s elapsed'.format(
                profile.N, profile.NB, profile.sched, profile.gflops, profile.time_elapsed) )
        figure.set_size_inches(10, 7)
        figure.savefig('stacked_thread_bars_{}.pdf'.format(profile.unique_name().rstrip('_')), dpi=300, bbox_inches='tight')
        print('done {}'.format(profile))
