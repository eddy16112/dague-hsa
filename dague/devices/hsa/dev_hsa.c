/*
 * Copyright (c) 2010-2015 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 */

#include "dague_config.h"
#include "dague/dague_internal.h"

#include "dague.h"
#include "dague/data_internal.h"
#include "dague/devices/hsa/dev_hsa.h"

static int dague_hsa_device_fini(dague_device_t* device);


int dague_hsa_init(dague_context_t *dague_context)
{
    int ndevices;
    int i;

    ndevices = 1;
    for (i = 0; i < ndevices; i++) {
        hsa_device_t *hsa_device;
        hsa_device = (hsa_device_t*)calloc(1, sizeof(hsa_device_t));
        OBJ_CONSTRUCT(hsa_device, dague_list_item_t);
        hsa_device->hsa_index = i;
        hsa_device->super.name = "HSA";
        hsa_device->super.type = DAGUE_DEV_HSA;

        OBJ_CONSTRUCT(&hsa_device->pending, dague_list_t);
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
    hsa_device_t* hsa_device = (hsa_device_t*)device;
    OBJ_DESTRUCT(&hsa_device->pending);
    free(hsa_device);

    return DAGUE_SUCCESS;
}
