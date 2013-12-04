#!/usr/bin/env python

""" PURE PYTHON interface to the PaRSEC Python Profiles generated by parsec_binprof.

The recommended shorthand for the name is "P3",
as in the 3 Ps of the alternative abbreviation "PPP".
Therefore, the preferred import method is "import parsec_profiling as p3"

This module is especially suitable for use when a separate Python program
has done the reading of the profile and has stored the profile in this format
in some sort of cross-process format. The format natively supported by pandas
and P3 is HDF5, and can be easily written to and read from using the functions
to_hdf and from_hdf.
"""

import sys
import os
import re
import shutil
import numpy as np
import pandas as pd

import warnings # because these warnings are annoying, and I can find no way around them.
warnings.filterwarnings('ignore', category=pd.io.pytables.PerformanceWarning)

p3_core = '.h5-'

class ParsecProfile(object):
    class_version = 1.0 # created 2013.10.22 after move to pandas
    basic_event_columns = ['node_id', 'thread_id',  'handle_id', 'type',
                           'begin', 'end', 'duration', 'flags', 'unique_id', 'id']
    HDF_TOP_LEVEL_NAMES = ['event_types', 'event_names', 'event_attributes',
                           'nodes', 'threads', 'information', 'errors']
    default_descriptors = ['hostname', 'exe', 'ncores', 'sched']
    @staticmethod
    def from_hdf(filename, skeleton_only=False):
        store = pd.HDFStore(filename, 'r')
        top_level = list()
        if not skeleton_only:
            events = store['events']
        else:
            events = None
        for name in ParsecProfile.HDF_TOP_LEVEL_NAMES:
            top_level.append(store[name])
        profile = ParsecProfile(events, *top_level)
        profile._store = store
        return profile

    # the init function should not ordinarily be used
    # it is better to use from_hdf(), from_native(), or autoload()
    def __init__(self, events, event_types, event_names, event_attributes,
                 nodes, threads, information, errors):
        self.__version__ = self.__class__.class_version
        # core data
        self.events = events
        self.event_types = event_types
        self.event_names = event_names
        self.event_attributes = event_attributes
        self.nodes = nodes
        self.threads = threads
        self.information = information
        self.errors = errors
        # metadata
        self.basic_columns = ParsecProfile.basic_event_columns
    def to_hdf(self, filename, table=False, append=False, complevel=0,
               complib='blosc'):
        store = pd.HDFStore(filename + '.tmp', 'w')
        for name in ParsecProfile.HDF_TOP_LEVEL_NAMES:
            store.put(name, self.__dict__[name])
        store.put('events', self.events, table=table, append=append)
        store.close()
        # do atomic move once it's finished writing,
        # so as to allow Ctrl-Cs without secret breakage
        shutil.move(filename + '.tmp', filename)
    # this allows certain 'acceptable' attribute abbreviations
    # and automatically searches the 'information' dictionary
    def __getattr__(self, name):
        if name == 'exe':
            try:
                m = re.match('.*testing_(\w+)', self.information['exe'])
                return m.group(1)
            except Exception as e:
                pass
        try:
            return self.information[name]
        except:
            pass
        try:
            return self.information[str(name).upper()]
        except:
            pass
        try:
            return self.information['PARAM_' + str(name).upper()]
        except:
            return object.__getattribute__(self, name)
        # potentially add one more set of 'known translations'
        # such as 'perf' -> 'gflops', 'ex' -> 'exname'
    def __repr__(self):
        return self.descrip()
    def descrip(self, infos=default_descriptors):
        desc = ''
        for info in infos:
            desc += str(self.__getattr__(info)) + ' '
        return desc[:-1]
    def name(self, infos=default_descriptors):
        return self.descrip(infos).replace(' ', '_')
    # use with care - does an eval() on self'user text' when 'user text' starts with '.'
    def filter_events(self, filter_strings):
        events = self.events
        for filter_str in filter_strings:
            key, value = filter_str.split('==')
            if str(value).startswith('.'):
                # do eval
                eval_str = 'self' + str(value)
                value = eval(eval_str)
            events = events[:][events[key] == value]
        return events

def find_profile_sets(profiles, on=['cmdline']): #['N', 'M', 'NB', 'MB', 'IB', 'sched', 'exe', 'hostname'] ):
    profile_sets = dict()
    for profile in profiles:
        name = ''
        for info in on:
            name += str(profile.__getattr__(info)).replace('/', '') + '_'
        try:
            name = name[:-1]
            profile_sets[name].append(profile)
        except:
            profile_sets[name] = [profile]
    return profile_sets

# Does a best-effort merge on sets of profiles.
# Merges only the events, threads, and nodes, along with
# the top-level "information" struct.
# Intended for use after 'find_profile_sets'
# dangerous for use with groups of profiles that do not
# really belong to a reasonably-defined set.
# In particular, the event_type, event_name, and event_attributes
# DataFrames are chosen from the first profile in the set - no
# attempt is made to merge them at this time.
def automerge_profile_sets(profile_sets):
    merged_profiles = list()
    for p_set in profile_sets:
        merged_profile = p_set[0]
        merged_info = p_set[0].information
        for profile in p_set[1:]:
            # ADD UNIQUE ID
            #
            # add start time as id to every row in events and threads DataFrames
            # so that it is still possible to 'split' the merged profile
            # based on start_time id, which should differ for every run...
            if profile == p_set[1]:
                start_time_array = np.empty(len(merged_profile.events), dtype=int)
                start_time_array.fill(merged_profile.start_time)
                merged_profile.events['start_time'] = pd.Series(start_time_array)
                merged_profile.threads['start_time'] = pd.Series(start_time_array[:len(merged_profile.threads)])
            start_time_array = np.empty(len(profile.events), dtype=int)
            start_time_array.fill(profile.start_time)
            events = profile.events
            events['start_time'] = pd.Series(start_time_array)
            threads = profile.threads
            threads['start_time'] = pd.Series(start_time_array[:len(threads)])
            # CONCATENATE EVENTS
            merged_profile.events = pd.concat([merged_profile.events, events])
            merged_profile.nodes = pd.concat([merged_profile.nodes, profile.nodes])
            merged_profile.threads = pd.concat([merged_profile.threads, threads])
            # INFORMATION MERGE
            #
            # drop values which are not present and equal in all dictionaries...
            # except for floating-point values, which we average!
            mult = 1.0 / len(p_set)
            for key, value in profile.information.iteritems():
                if key not in merged_info: # not present
                    merged_info.drop(key)
                elif value != merged_info[key]:
                    try:
                        temp_fl = float(value)
                        if '.' in str(value):
                            # do average
                            if profile == p_set[1]:
                                merged_info[key] = merged_info[key] * mult
                            merged_info[key] += value * mult
                        else: # not float
                            merged_info.drop(key)
                    except: # not equal and not float
                        merged_info.drop(key)
        merged_profile.information = merged_info
        merged_profiles.append(merged_profile)
    return merged_profiles
