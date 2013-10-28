#!/usr/bin/env python

# this is a PURE PYTHON interface to
# pure Python objects generated by parsec_binprof.

# It is especially suitable for use when a separate
# Python program has done the reading of the profile
# and stored the profile in this format in some sort
# of cross-process format, such as a pickle.

import sys, os
import pandas as pd

class ParsecProfile(object):
    class_version = 1.0 # created 2013.10.22 after move to pandas
    basic_event_columns = ['node_id', 'thread_id',  'handle_id', 'key',
                           'begin', 'end', 'duration', 'flags', 'unique_id', 'id']
    HDF_TOP_LEVEL_NAMES = ['event_types', 'type_names', 'nodes',
                           'threads', 'errors', 'information']
    HDF_ATTRIBUTE_NAMES = ['ex', 'N', 'cores', 'NB', 'IB', 'sched', 'perf', 'time']
    @staticmethod
    def from_hdf(filename):
        store = pd.HDFStore(filename, 'r')
        top_level = list()
        for name in ParsecProfile.HDF_TOP_LEVEL_NAMES:
            top_level.append(store[name])
        profile = ParsecProfile(store['events'], *top_level)
        store.close()
        return profile

    # the init function should not ordinarily be used
    # it is better to use from_hdf(), from_native(), or autoload()
    def __init__(self, events, event_types, type_names,
                 nodes, threads, errors, information):
        self.__version__ = self.__class__.class_version
        # core data
        self.events = events
        self.event_types = event_types
        self.type_names = type_names
        self.nodes = nodes
        self.threads = threads
        self.information = information
        self.errors = errors
        # metadata
        self.basic_columns = ParsecProfile.basic_event_columns
    def to_hdf(self, filename, table=False, append=False):
        store = pd.HDFStore(filename, 'w')
        for name in ParsecProfile.HDF_TOP_LEVEL_NAMES:
            store.put(name, self.__dict__[name])
        store.put('events', self.events, table=table, append=append)
        return store
    # this allows certain 'acceptable' attribute abbreviations
    # and automatically searches the 'information' dictionary
    def __getattr__(self, name):
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
