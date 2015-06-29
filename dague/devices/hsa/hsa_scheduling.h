/*
 * Copyright (c) 2010-2015 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 */

#include "dague_config.h"

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

    hsa_device = (hsa_device_t*)dague_devices_get(which_gpu);

    /* Check the GPU status */
    rc = dague_atomic_inc_32b( &(hsa_device->mutex) );
    if( 1 != rc ) {  /* I'm not the only one messing with this GPU */
        dague_fifo_push( &(hsa_device->pending), (dague_list_item_t*)this_task );
        return DAGUE_HOOK_RETURN_ASYNC;
    }

exec_task:
    printf("run task %p\n", this_task);
    hsa_kernel_submit(hsa_device, this_task, NULL);

    __dague_complete_execution( eu_context, this_task->ec );
    free(this_task);
    this_task = NULL;
   
fetch_task_from_shared_queue: 
    this_task = (dague_hsa_context_t*)dague_fifo_try_pop( &(hsa_device->pending) );
    if( NULL != this_task ) {
        DEBUG2(( "HSA[%1d]:\tGet from shared queue %s priority %d\n", hsa_device->hsa_index,
                 dague_snprintf_execution_context(tmp, MAX_TASK_STRLEN, this_task->ec),
                 this_task->ec->priority ));
        printf("task %p fetched from queue\n", this_task);
        goto exec_task; 
    }

    rc = dague_atomic_dec_32b( &(hsa_device->mutex) );
    if( 0 == rc ) {  /* I was the last one */
        return DAGUE_HOOK_RETURN_ASYNC;
    }
    goto fetch_task_from_shared_queue;
}
