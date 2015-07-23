/*
 * Copyright (c) 2010-2015 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 */

#include "dague_config.h"
#include "dague/dague_internal.h"
#include "dague/sys/atomic.h"

#include "dague/utils/mca_param.h"
#include "dague/constants.h"

#if defined (HAVE_HSA)

#include "dague.h"
#include "dague/data_internal.h"
#include "dague/devices/hsa/dev_hsa.h"
#include "dague/profiling.h"
#include "dague/execution_unit.h"
#include "dague/arena.h"
#include "dague/utils/output.h"
#include "dague/utils/argv.h"

#include <atmi_rt.h>

#if defined(DAGUE_PROF_TRACE)
/* Accepted values are: DAGUE_PROFILE_HSA_TRACK_EXEC
 */
int dague_hsa_trackable_events = DAGUE_PROFILE_HSA_TRACK_EXEC | DAGUE_PROFILE_CUDA_TRACK_OWN;

int dague_cuda_own_GPU_key_start;
int dague_cuda_own_GPU_key_end;
#endif  /* defined(PROFILING) */

int dague_hsa_output_stream = -1;

static int dague_hsa_device_fini(dague_device_t* device);


int dague_hsa_init(dague_context_t *dague_context)
{
    int ndevices;
    int i, j, k;

#if defined(DAGUE_PROF_TRACE)
    dague_profiling_add_dictionary_keyword( "hsa", "fill:#66ff66",
                                            0, NULL,
                                            &dague_hsa_own_GPU_key_start, &dague_hsa_own_GPU_key_end);
#endif  /* defined(PROFILING) */

    ndevices = 1;
    for (i = 0; i < ndevices; i++) {
        hsa_device_t *hsa_device;
        hsa_device = (hsa_device_t*)calloc(1, sizeof(hsa_device_t));
        OBJ_CONSTRUCT(hsa_device, dague_list_item_t);
        hsa_device->hsa_index = i;
        hsa_device->super.name = "HSA";
        hsa_device->super.type = DAGUE_DEV_HSA;

        OBJ_CONSTRUCT(&hsa_device->pending, dague_list_t);
        
        hsa_device->max_exec_streams = DAGUE_HSA_MAX_STREAMS;
        hsa_device->exec_stream = (dague_hsa_exec_stream_t*)malloc(hsa_device->max_exec_streams * sizeof(dague_hsa_exec_stream_t)); 
        for( j = 0; j < hsa_device->max_exec_streams; j++ ) {
            dague_hsa_exec_stream_t* exec_stream = &(hsa_device->exec_stream[j]);

            /* Allocate the stream */
            exec_stream->atmi_stream = (atmi_stream_t *)malloc(sizeof(atmi_stream_t));
            exec_stream->atmi_stream->ordered = ATMI_ORDERED;
            exec_stream->max_events   = DAGUE_HSA_MAX_TASKS_PER_STREAM;
            exec_stream->executed     = 0;
            exec_stream->start        = 0;
            exec_stream->end          = 0;
            exec_stream->fifo_pending = (dague_list_t*)OBJ_NEW(dague_list_t);
            OBJ_CONSTRUCT(exec_stream->fifo_pending, dague_list_t);
            exec_stream->tasks  = (dague_hsa_context_t**)malloc(exec_stream->max_events
                                                                * sizeof(dague_hsa_context_t*));
            for (k = 0; k < DAGUE_HSA_MAX_TASKS_PER_STREAM; k++) {
                exec_stream->tasks[k] = NULL;
            }
#if defined(DAGUE_PROF_TRACE)
            exec_stream->profiling = dague_profiling_thread_init( 2*1024*1024, DAGUE_PROFILE_STREAM_STR, i, j );
            exec_stream->prof_event_track_enable = dague_hsa_trackable_events & DAGUE_PROFILE_HSA_TRACK_EXEC;
            exec_stream->prof_event_key_start    = -1;
            exec_stream->prof_event_key_end      = -1;
#endif  /* defined(DAGUE_PROF_TRACE) */
        }
        

        dague_devices_add(dague_context, &(hsa_device->super));
        printf("hsa device %d init\n", i);
    }
    return DAGUE_SUCCESS;
}

int dague_hsa_fini(void)
{
    hsa_device_t* hsa_device;
    int i;

    for(i = 0; i < dague_devices_enabled(); i++) {
        if( NULL == (hsa_device = (hsa_device_t*)dague_devices_get(i)) ) continue;
        if(DAGUE_DEV_HSA != hsa_device->super.type) continue;

        dague_hsa_device_fini((dague_device_t*)hsa_device);
        dague_device_remove((dague_device_t*)hsa_device);
        printf("hsa device %d fini\n", i);
    }

    return DAGUE_SUCCESS;
}

static int dague_hsa_device_fini(dague_device_t* device)
{
    int j;
    hsa_device_t* hsa_device = (hsa_device_t*)device;
    OBJ_DESTRUCT(&hsa_device->pending);
   
    for( j = 0; j < hsa_device->max_exec_streams; j++ ) {
        dague_hsa_exec_stream_t* exec_stream = &(hsa_device->exec_stream[j]);

        exec_stream->max_events   = DAGUE_HSA_MAX_TASKS_PER_STREAM;
        exec_stream->executed     = 0;
        exec_stream->start        = 0;
        exec_stream->end          = 0;

        free(exec_stream->tasks); exec_stream->tasks = NULL;
        free(exec_stream->fifo_pending); exec_stream->fifo_pending = NULL;
        free(exec_stream->atmi_stream);
    } 
    free(hsa_device->exec_stream); hsa_device->exec_stream = NULL;
   
    free(hsa_device);

    return DAGUE_SUCCESS;
}

#if DAGUE_GPU_USE_PRIORITIES
static inline dague_list_item_t* dague_fifo_push_ordered( dague_list_t* fifo,
                                                          dague_list_item_t* elem )
{
    dague_ulist_push_sorted(fifo, elem, dague_execution_context_priority_comparator);
    return elem;
}
#define DAGUE_FIFO_PUSH  dague_fifo_push_ordered
#else
#define DAGUE_FIFO_PUSH  dague_ulist_fifo_push
#endif

int progress_stream( hsa_device_t* hsa_device,
                     dague_hsa_exec_stream_t* exec_stream,
                     advance_task_function_t progress_fct,
                     dague_hsa_context_t* task,
                     dague_hsa_context_t** out_task )
{
    int saved_rc = 0, rc;
    *out_task = NULL;

    if( NULL != task ) {
        DAGUE_FIFO_PUSH(exec_stream->fifo_pending, (dague_list_item_t*)task);
        task = NULL;
    }
 grab_a_task:
    if( NULL == exec_stream->tasks[exec_stream->start] ) {
        /* get the best task */
        task = (dague_hsa_context_t*)dague_ulist_fifo_pop(exec_stream->fifo_pending);
    }
    if( NULL == task ) {
        /* No more room on the event list or no tasks. Keep moving */
        goto check_completion;
    }
    DAGUE_LIST_ITEM_SINGLETON((dague_list_item_t*)task);

    assert( NULL == exec_stream->tasks[exec_stream->start] );
    /**
     * In case the task is succesfully progressed, the corresponding profiling
     * event is triggered.
     */
    rc = progress_fct( hsa_device, task, exec_stream );
    if( 0 > rc ) {
        if( -1 == rc ) return -1;  /* Critical issue */
        /* No more room on the GPU. Push the task back on the queue and check the completion queue. */
        DAGUE_FIFO_PUSH(exec_stream->fifo_pending, (dague_list_item_t*)task);
        DAGUE_OUTPUT_VERBOSE((3, dague_hsa_output_stream,
                              "GPU: Reschedule %s(task %p) priority %d: task can not be enqueued\n",
                              task->ec->function->name, (void*)task->ec, task->ec->priority ));
        saved_rc = rc;  /* keep the info for the upper layer */
    } else {
        /**
         * Do not skip the cuda event generation. The problem is that some of the inputs
         * might be in the pipe of being transferred to the GPU. If we activate this task
         * too early, it might get executed before the data is available on the GPU.
         * Obviously, this lead to incorrect results.
         */
        exec_stream->tasks[exec_stream->start] = task;
        exec_stream->start = (exec_stream->start + 1) % exec_stream->max_events;
#if DAGUE_OUTPUT_VERBOSE >= 3
        DAGUE_OUTPUT_VERBOSE((3, dague_hsa_output_stream,
                              "GPU: Submitted %s(task %p) priority %d on stream %p\n",
                              task->ec->function->name, (void*)task->ec, task->ec->priority,
                              (void*)exec_stream->cuda_stream));
        
#endif
    }
    task = NULL;
    
check_completion:
    if( (NULL == *out_task) && (NULL != exec_stream->tasks[exec_stream->end]) ) {
       // printf("I query stream %p, task %p, atmi_task %p\n", exec_stream, exec_stream->tasks[exec_stream->end], exec_stream->tasks[exec_stream->end]->atmi_task);
        rc = snk_task_query(exec_stream->tasks[exec_stream->end]->atmi_task);
        if( 0 == rc ) {
            /* Save the task for the next step */
            task = *out_task = exec_stream->tasks[exec_stream->end];
#if DAGUE_OUTPUT_VERBOSE >= 3
            DAGUE_OUTPUT_VERBOSE((3, dague_hsa_output_stream,
                                  "GPU: Completed %s(task %p) priority %d on stream %p\n",
                                  task->ec->function->name, (void*)task->ec, task->ec->priority,
                                  (void*)exec_stream->cuda_stream));
            
#endif
            exec_stream->tasks[exec_stream->end] = NULL;
            exec_stream->end = (exec_stream->end + 1) % exec_stream->max_events;
#if defined(DAGUE_PROF_TRACE)
            if( exec_stream->prof_event_track_enable ) {
                DAGUE_TASK_PROF_TRACE(exec_stream->profiling,
                                      (-1 == exec_stream->prof_event_key_end ?
                                      DAGUE_PROF_FUNC_KEY_END(task->ec->dague_handle,
                                                                task->ec->function->function_id) :
                                      exec_stream->prof_event_key_end),
                                      task->ec);
            }
#endif /* (DAGUE_PROF_TRACE) */
            task = NULL;  /* Try to schedule another task */
            goto grab_a_task;
        }
    }
    return saved_rc;
}

#endif /* HAVE_HSA*/
