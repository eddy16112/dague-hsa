/*
 * Copyright (c) 2009      The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 */
#ifndef __USE_ARENA_H__
#define __USE_ARENA_H__

#include <stdlib.h>

#include "dague_config.h"
#include "dague_internal.h"
#if defined(HAVE_STDDEF_H)
#include <stddef.h>
#endif  /* HAVE_STDDEF_H */
#include "debug.h"
#include "stats.h"

#include <dague/sys/atomic.h>
#include "lifo.h"

#include "remote_dep.h"

#define DAGUE_ALIGN(x,a,t) (((x)+((t)(a)-1)) & ~(((t)(a)-1)))
#define DAGUE_ALIGN_PTR(x,a,t) ((t)DAGUE_ALIGN((uintptr_t)x, a, uintptr_t))
#define DAGUE_ALIGN_PAD_AMOUNT(x,s) ((~((uintptr_t)(x))+1) & ((uintptr_t)(s)-1))

struct dague_arena_s {
    size_t alignment;                        /* alignment to be respected, elem_size should be >> alignment, prefix size is the minimum alignment */
    size_t elem_size;                        /* size of one element (unpacked in memory, aka extent) */
    dague_remote_dep_datatype_t opaque_dtt;  /* the appropriate type for the network engine to send an element */
    dague_lifo_t area_lifo;
    volatile int32_t used;                   /* elements currently out of the arena */
    int32_t max_used;                        /* maximum size of the arena in elements */
    volatile int32_t released;               /* elements currently not used but allocated */
    int32_t max_released;                    /* when more that max elements are released, they are really freed instead of joining the lifo
                                              * some host hardware requires special allocation functions (Cuda, pinning,
                                              * Open CL, ...). Defaults are to use C malloc/free */
    dague_data_allocate_t data_malloc;
    dague_data_free_t data_free;
};

struct dague_arena_chunk_s {
    dague_list_item_t item;                  /* chaining of this chunk when in an arena's free list.
                                              *   SINGLETON when ( (not in free list) and (in debug mode) ) */
    dague_arena_t* origin;
    void*          data;
    uint32_t refcount;
    uint32_t count;
};

/* for SSE, 16 is mandatory, most cache are 64 bit aligned */
#define DAGUE_ARENA_ALIGNMENT_64b 8
#define DAGUE_ARENA_ALIGNMENT_INT sizeof(int)
#define DAGUE_ARENA_ALIGNMENT_PTR sizeof(void*)
#define DAGUE_ARENA_ALIGNMENT_SSE 16
#define DAGUE_ARENA_ALIGNMENT_CL1 64

int dague_arena_construct(dague_arena_t* arena,
                          size_t elem_size,
                          size_t alignment,
                          dague_remote_dep_datatype_t opaque_dtt);
void dague_arena_destruct(dague_arena_t* arena);

dague_data_t* dague_arena_get(dague_arena_t* arena, size_t count);
void dague_arena_release(dague_data_t* ptr);

#define CHUNK_DATA(CHK) \
    (assert(NULL != ((CHK)->data)), (CHK)->data)

#endif /* __USE_ARENA_H__ */

