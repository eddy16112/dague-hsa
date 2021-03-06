/*
 * Copyright (c) 2009      The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 */

#ifndef ATOMIC_H_HAS_BEEN_INCLUDED
#define ATOMIC_H_HAS_BEEN_INCLUDED

#include "dague_config.h"
#include <stdint.h>
#include <unistd.h>

#if defined(DAGUE_ATOMIC_USE_GCC_32_BUILTINS)
#include "atomic-gcc.h"
#elif defined(MAC_OS_X)
#include "atomic-macosx.h"
#elif defined(ARCH_X86)
#include "atomic-x86_32.h"
#elif defined(ARCH_X86_64)
#include "atomic-x86_64.h"
#elif defined(ARCH_PPC)
#include "atomic-ppc.h"
#else
#error "Using unsafe atomics"
#endif

#include <assert.h>

static inline int dague_atomic_cas_xxb( volatile void* location,
                                          uint64_t old_value,
                                          uint64_t new_value,
                                          size_t type_size )
{
    switch(type_size){
    case 4:
        return dague_atomic_cas_32b( (volatile uint32_t*)location,
                                       (uint32_t)old_value, (uint32_t)new_value );
    case 8:
        return dague_atomic_cas_64b( (volatile uint64_t*)location,
                                       (uint64_t)old_value, (uint64_t)new_value );
    }
    return 0;
}

static inline uint64_t dague_atomic_bor_xxb( volatile void* location,
                                               uint64_t or_value,
                                               size_t type_size )
{
    assert( 4 == type_size );
	(void)type_size;
    return (uint64_t)dague_atomic_bor_32b( (volatile uint32_t*)location,
                                             (uint32_t)or_value);
}

#define dague_atomic_band(LOCATION, OR_VALUE)  \
    (__typeof__(*(LOCATION)))dague_atomic_band_xxb(LOCATION, OR_VALUE, sizeof(*(LOCATION)) )

#define dague_atomic_bor(LOCATION, OR_VALUE)  \
    (__typeof__(*(LOCATION)))dague_atomic_bor_xxb(LOCATION, OR_VALUE, sizeof(*(LOCATION)) )

#define dague_atomic_cas(LOCATION, OLD_VALUE, NEW_VALUE)              \
    dague_atomic_cas_xxb((volatile void*)(LOCATION),              \
                           (uint64_t)(OLD_VALUE), (uint64_t)(NEW_VALUE), \
                           sizeof(*(LOCATION)))                         \
    
#define dague_atomic_set_mask(LOCATION, MASK) dague_atomic_bor((LOCATION), (MASK))
#define dague_atomic_clear_mask(LOCATION, MASK)  dague_atomic_band((LOCATION), ~(MASK))

#ifndef DAGUE_ATOMIC_HAS_ATOMIC_INC_32B
static inline uint32_t dague_atomic_inc_32b( volatile uint32_t *location )
{
    uint32_t l;
    do {
        l = *location;
    } while( !dague_atomic_cas_32b( location, l, l+1 ) );
    return l+1;
}
#endif  /* DAGUE_ATOMIC_HAS_ATOMIC_INC_32B */

#ifndef DAGUE_ATOMIC_HAS_ATOMIC_DEC_32B
static inline uint32_t dague_atomic_dec_32b( volatile uint32_t *location )
{
    uint32_t l;
    do {
        l = *location;
    } while( !dague_atomic_cas_32b( location, l, l-1 ) );
    return l-1;
}
#endif  /* DAGUE_ATOMIC_HAS_ATOMIC_DEC_32B */

#endif  /* ATOMIC_H_HAS_BEEN_INCLUDED */
