/*
 * Copyright (c) 2010-2014 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 */

#include <dague_config.h>
#include "arena.h"
#include "dague/class/lifo.h"
#include "data.h"

#if defined(DAGUE_PROF_TRACE) && defined(DAGUE_PROF_TRACE_ACTIVE_ARENA_SET)

#include "profiling.h"
extern int arena_memory_alloc_key, arena_memory_free_key;
extern int arena_memory_used_key, arena_memory_unused_key;
#define TRACE_MALLOC(key, size, ptr) dague_profiling_ts_trace(key, (uint64_t)ptr, PROFILE_OBJECT_ID_NULL, &size)
#define TRACE_FREE(key, ptr)         dague_profiling_ts_trace(key, (uint64_t)ptr, PROFILE_OBJECT_ID_NULL, NULL)
#else
#define TRACE_MALLOC(key, size, ptr) do {} while (0)
#define TRACE_FREE(key, ptr) do {} while (0)
#endif

#define DAGUE_ARENA_MIN_ALIGNMENT(align) ((ptrdiff_t)(align*((sizeof(dague_arena_chunk_t)-1)/align+1)))

size_t dague_arena_max_allocated_memory = 0;  /* unlimited */
size_t dague_arena_max_cached_memory    = 0;  /* unlimitted */

int dague_arena_construct_ex(dague_arena_t* arena,
                             size_t elem_size,
                             size_t alignment,
                             dague_datatype_t opaque_dtt,
                             size_t max_allocated_memory,
                             size_t max_cached_memory)
{
    /* alignment must be more than zero and power of two */
    if( (alignment <= 1) || (alignment & (alignment - 1)) )
        return -1;

    assert(0 == (((uintptr_t)arena) % sizeof(uintptr_t))); /* is it aligned */

    OBJ_CONSTRUCT(&arena->area_lifo, dague_lifo_t);
    arena->alignment    = alignment;
    arena->elem_size    = elem_size;
    arena->opaque_dtt   = opaque_dtt;
    arena->used         = 0;
    arena->max_used     = max_allocated_memory / elem_size;
    arena->released     = 0;
    arena->max_released = max_cached_memory / elem_size;

    arena->data_malloc  = dague_data_allocate;
    arena->data_free    = dague_data_free;
    return 0;
}

int dague_arena_construct(dague_arena_t* arena,
                          size_t elem_size,
                          size_t alignment,
                          dague_datatype_t opaque_dtt)
{
    return dague_arena_construct_ex(arena, elem_size,
                                    alignment, opaque_dtt,
                                    dague_arena_max_allocated_memory,
                                    dague_arena_max_cached_memory);
}

void dague_arena_destruct(dague_arena_t* arena)
{
    dague_list_item_t* item;

    assert(0 == arena->used);

    while(NULL != (item = dague_lifo_pop(&arena->area_lifo))) {
        DEBUG3(("Arena:\tfree element base ptr %p, data ptr %p (from arena %p)\n",
                item, ((dague_arena_chunk_t*)item)->data, arena));
        TRACE_FREE(arena_memory_free_key, item);
        arena->data_free(item);
    }
    OBJ_DESTRUCT(&arena->area_lifo);
}

static inline dague_list_item_t*
dague_arena_get_chunk( dague_arena_t *arena, size_t size, dague_data_allocate_t alloc )
{
    dague_lifo_t *list = &arena->area_lifo;
    dague_list_item_t *item;
    item = dague_lifo_pop(list);
    if( NULL == item ) {
        if(arena->max_used > 0) {
            int32_t current = dague_atomic_add_32b(&arena->used, 1);
            if(current > arena->max_used) {
                dague_atomic_dec_32b((uint32_t*)&arena->used);
                return NULL;
            }
        }
        if( size < sizeof( dague_list_item_t ) )
            size = sizeof( dague_list_item_t );
        item = (dague_list_item_t *)alloc( size );
        TRACE_MALLOC(arena_memory_alloc_key, size, item);
        OBJ_CONSTRUCT(item, dague_list_item_t);
        assert(NULL != item);
    }
    return item;
}

static void
dague_arena_release_chunk(dague_arena_t* arena,
                          dague_arena_chunk_t *chunk)
{
    TRACE_FREE(arena_memory_unused_key, chunk);

    if(arena->max_used > 0)
        dague_atomic_dec_32b((uint32_t*)&arena->used);

    if(chunk->count > 1 || arena->released >= arena->max_released) {
        DEBUG2(("Arena:\tdeallocate a tile of size %zu x %zu from arena %p, aligned by %zu, base ptr %p, data ptr %p, sizeof prefix %zu(%zd)\n",
                arena->elem_size, chunk->count, arena, arena->alignment, chunk, chunk->data, sizeof(dague_arena_chunk_t),
                DAGUE_ARENA_MIN_ALIGNMENT(arena->alignment)));
        TRACE_FREE(arena_memory_free_key, chunk);
        arena->data_free(chunk);
    } else {
        DEBUG2(("Arena:\tpush a data of size %zu from arena %p, aligned by %zu, base ptr %p, data ptr %p, sizeof prefix %zu(%zd)\n",
                arena->elem_size, arena, arena->alignment, chunk, chunk->data, sizeof(dague_arena_chunk_t),
                DAGUE_ARENA_MIN_ALIGNMENT(arena->alignment)));
        if(arena->max_released > 0) {
            dague_atomic_inc_32b((uint32_t*)&arena->released);
        }
        dague_lifo_push(&arena->area_lifo, &chunk->item);
    }
}

dague_data_copy_t *dague_arena_get_copy(dague_arena_t *arena, size_t count, int device)
{
    dague_arena_chunk_t *chunk;
    dague_data_t *data;
    dague_data_copy_t *copy;
    size_t size;

    if( count == 1 ) {
        size = DAGUE_ALIGN(arena->elem_size + arena->alignment + sizeof(dague_arena_chunk_t),
                           arena->alignment, size_t);
        chunk = (dague_arena_chunk_t *)dague_arena_get_chunk( arena, size, arena->data_malloc );
    } else {
        assert(count > 1);
        size = DAGUE_ALIGN(arena->elem_size * count + arena->alignment + sizeof(dague_arena_chunk_t),
                           arena->alignment, size_t);
        chunk = (dague_arena_chunk_t*)arena->data_malloc(size);
        TRACE_MALLOC(arena_memory_alloc_key, size, chunk);
    }
    if(NULL == chunk) return NULL;  /* no more */

#if defined(DAGUE_DEBUG_ENABLE)
    DAGUE_LIST_ITEM_SINGLETON( &chunk->item );
#endif
    TRACE_MALLOC(arena_memory_used_key, size, chunk);

    chunk->origin = arena;
    chunk->count = count;
    chunk->data = DAGUE_ALIGN_PTR( ((ptrdiff_t)chunk + sizeof(dague_arena_chunk_t)),
                                   arena->alignment, void* );

    assert(0 == (((ptrdiff_t)chunk->data) % arena->alignment));
    assert((arena->elem_size + (ptrdiff_t)chunk->data)  <= (size + (ptrdiff_t)chunk));

    data = dague_data_new();
    if( NULL == data ) {
        dague_arena_release_chunk(arena, chunk);
        return NULL;
    }

    data->nb_elts = count * arena->elem_size;

    copy = dague_data_copy_new( data, device );
    copy->flags |= DAGUE_DATA_FLAG_ARENA;
    copy->device_private = chunk->data;
    copy->arena_chunk = chunk;

    /* This data is going to be released once all copies are released
     * It does not exist without at least a copy, and we don't give the
     * pointer to the user, so we must remove our retain from it
     */
    OBJ_RELEASE(data);

    return copy;
}

void dague_arena_release(dague_data_copy_t* copy)
{
    dague_data_t *data;
    dague_arena_chunk_t *chunk;
    dague_arena_t* arena;

    data  = copy->original;
    chunk = copy->arena_chunk;
    arena = chunk->origin;

    assert(NULL != arena);
    assert(0 == (((uintptr_t)arena)%sizeof(uintptr_t))); /* is it aligned */

    if( NULL != data )
        dague_data_copy_detach( data, copy, 0 );

    dague_arena_release_chunk(arena, chunk);
}
