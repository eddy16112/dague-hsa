#include "dague.h"
#include <data_distribution.h>
#include <arena.h>

#if defined(HAVE_MPI)
#include <mpi.h>
static MPI_Datatype block;
#endif
#include <stdio.h>

#include "choice.h"
#include "choice_wrapper.h"

/**
 * @param [IN] A    the data, already distributed and allocated
 * @param [IN] size size of each local data element
 * @param [IN] nb   number of iterations
 *
 * @return the dague object to schedule.
 */
dague_object_t *choice_new(dague_ddesc_t *A, int size, dague_ddesc_t *decision, int nb, int world)
{
    dague_choice_object_t *o = NULL;
    
    if( nb <= 0 || size <= 0 ) {
        fprintf(stderr, "To work, CHOICE nb and size must be > 0\n");
        return (dague_object_t*)o;
    }

    o = dague_choice_new(A, nb, world, decision);

#if defined(HAVE_MPI)
    {
    	MPI_Type_vector(1, size, size, MPI_BYTE, &block);
        MPI_Type_commit(&block);
        dague_arena_construct(o->arenas[DAGUE_choice_DEFAULT_ARENA],
                              size * sizeof(char), size * sizeof(char), 
                              block);
        dague_arena_construct(o->arenas[DAGUE_choice_DECISION_ARENA],
                              sizeof(int), sizeof(int), 
                              MPI_INT);
    }
#endif

    return (dague_object_t*)o;
}

/**
 * @param [INOUT] o the dague object to destroy
 */
void choice_destroy(dague_object_t *o)
{
    dague_choice_object_t *c = (dague_choice_object_t*)o;

#if defined(HAVE_MPI)
    MPI_Type_free( &block );
#endif

    dague_choice_destroy( c );
}
