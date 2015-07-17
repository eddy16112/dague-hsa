/*
 * Copyright (c) 2010-2015 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 */

#include "dague_config.h"

#if defined (HAVE_HSA)

#include "dague/dague_internal.h"
#include "dague/devices/hsa/dev_hsa.h"
#include "dague/profiling.h"
#include "dague/execution_unit.h"
#include "dague/scheduling.h"

#include <errno.h>
#include "dague/class/lifo.h"

/**
 * Define functions names
 */
#ifndef KERNEL_NAME
#error "KERNEL_NAME must be defined before to include this file"
#endif

#define GENERATE_NAME_v2( _func_, _kernel_ ) _func_##_##_kernel_
#define GENERATE_NAME( _func_, _kernel_ ) GENERATE_NAME_v2( _func_, _kernel_ )

#define hsa_kernel_scheduler GENERATE_NAME( hsa_kernel_scheduler, KERNEL_NAME )
#define hsa_kernel_submit    GENERATE_NAME( hsa_kernel_submit   , KERNEL_NAME )

static inline dague_hook_return_t
hsa_kernel_scheduler( dague_execution_unit_t *eu_context,
                      dague_hsa_context_t    *this_task,
                      int which_gpu )
{
    hsa_device_t *hsa_device;
    int rc;
    int exec_stream = 0;
    dague_hsa_context_t *progress_task;

    hsa_device = (hsa_device_t*)dague_devices_get(which_gpu);

    /* Check the GPU status */
    rc = dague_atomic_inc_32b( &(hsa_device->mutex) );
    if( 1 != rc ) {  /* I'm not the only one messing with this GPU */
        dague_fifo_push( &(hsa_device->pending), (dague_list_item_t*)this_task );
        return DAGUE_HOOK_RETURN_ASYNC;
    }

check_in_deps:
//    printf("run task %p\n", this_task);
    exec_stream = (exec_stream + 1) % (hsa_device->max_exec_streams);  /* Choose an exec_stream */
 //   hsa_kernel_submit(hsa_device, this_task, &(hsa_device->exec_stream[exec_stream]));
    rc = progress_stream( hsa_device,
                          &(hsa_device->exec_stream[exec_stream]),
                          hsa_kernel_submit,
                          this_task, &progress_task );
    
    if( NULL != progress_task ) {
        /* We have a succesfully completed task. However, it is not this_task, as
         * it was just submitted into the data retrieval system. Instead, the task
         * ready to move into the next level is the progress_task.
         */
        this_task = progress_task;
        progress_task = NULL;
        goto complete_task;
    }
   
fetch_task_from_shared_queue: 
    this_task = (dague_hsa_context_t*)dague_fifo_try_pop( &(hsa_device->pending) );
    if( NULL != this_task ) {
        DEBUG2(( "HSA[%1d]:\tGet from shared queue %s priority %d\n", hsa_device->hsa_index,
                 dague_snprintf_execution_context(tmp, MAX_TASK_STRLEN, this_task->ec),
                 this_task->ec->priority ));
//        printf("task %p fetched from queue\n", this_task);
    }
    goto check_in_deps; 

complete_task:
    assert( NULL != this_task );
    hsa_device->super.executed_tasks++;
    DAGUE_LIST_ITEM_SINGLETON(this_task);
    __dague_complete_execution( eu_context, this_task->ec );
    free(this_task);

    rc = dague_atomic_dec_32b( &(hsa_device->mutex) );
    if( 0 == rc ) {  /* I was the last one */
        return DAGUE_HOOK_RETURN_ASYNC;
    }
    this_task = progress_task;
    goto fetch_task_from_shared_queue;
}

#endif /* HAVE_HSA */
