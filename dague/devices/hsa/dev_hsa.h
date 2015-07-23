/*
 * Copyright (c) 2010-2013 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 */

#ifndef DAGUE_DEVICE_HSA_H_HAS_BEEN_INCLUDED
#define DAGUE_DEVICE_HSA_H_HAS_BEEN_INCLUDED

#include "dague_config.h"
#include "dague/dague_internal.h"
#include "dague/class/dague_object.h"
#include "dague/devices/device.h"

#if defined(HAVE_HSA)

#include "dague/class/list_item.h"
#include "dague/class/list.h"

#include <atmi.h>

BEGIN_C_DECLS

#define DAGUE_HSA_MAX_STREAMS            4
#define DAGUE_HSA_MAX_TASKS_PER_STREAM   10000

#if defined(DAGUE_PROF_TRACE)
#define DAGUE_PROFILE_HSA_TRACK_OWN      0x0004
#define DAGUE_PROFILE_HSA_TRACK_EXEC     0x0008

extern int dague_hsa_trackable_events;
extern int dague_hsa_own_GPU_key_start;
extern int dague_hsa_own_GPU_key_end;
#endif  /* defined(PROFILING) */

typedef struct __dague_hsa_context {
    dague_list_item_t          list_item;
    dague_execution_context_t *ec;
    int task_type;
    atmi_task_t *atmi_task;
} dague_hsa_context_t;

typedef struct __dague_hsa_exec_stream {
    struct __dague_hsa_context **tasks;
    int32_t max_events;
    atmi_stream_t *atmi_stream;
    int32_t executed;    /* number of executed tasks */
    int32_t start, end;  /* circular buffer management start and end positions */
    dague_list_t *fifo_pending;
#if defined(DAGUE_PROF_TRACE)
    dague_thread_profiling_t *profiling;
#endif  /* defined(PROFILING) */
#if defined(DAGUE_PROF_TRACE)
    int prof_event_track_enable;
    int prof_event_key_start, prof_event_key_end;
#endif  /* defined(PROFILING) */
} dague_hsa_exec_stream_t;

typedef struct _hsa_device {
    dague_device_t super;
    uint8_t hsa_index;
    uint8_t max_exec_streams;
    volatile uint32_t mutex;
    dague_hsa_exec_stream_t* exec_stream;
    dague_list_t pending;
} hsa_device_t;

END_C_DECLS

int dague_hsa_init(dague_context_t *dague_context);
int dague_hsa_fini(void);

typedef int (*advance_task_function_t)(hsa_device_t* hsa_device,
                                       dague_hsa_context_t* task,
                                       dague_hsa_exec_stream_t* hsa_stream);

int progress_stream( hsa_device_t* hsa_device,
                     dague_hsa_exec_stream_t* hsa_stream,
                     advance_task_function_t progress_fct,
                     dague_hsa_context_t* task,
                     dague_hsa_context_t** out_task );

#endif /*HAVE_HSA */

#endif /* DAGUE_DEVICE_HSA_H_HAS_BEEN_INCLUDED */
