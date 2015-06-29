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

#include "dague/class/list_item.h"
#include "dague/class/list.h"

BEGIN_C_DECLS

typedef struct __dague_hsa_context {
    dague_list_item_t          list_item;
    dague_execution_context_t *ec;
    int task_type;
} dague_hsa_context_t;

typedef struct __dague_hsa_exec_stream {
    int32_t max_events;
} dague_hsa_exec_stream_t;

typedef struct _hsa_device {
    dague_device_t super;
    uint8_t hsa_index;
    volatile uint32_t mutex;
    dague_list_t pending;
} hsa_device_t;

END_C_DECLS

int dague_hsa_init(dague_context_t *dague_context);
int dague_hsa_fini(void);

#endif /* DAGUE_DEVICE_HSA_H_HAS_BEEN_INCLUDED */
