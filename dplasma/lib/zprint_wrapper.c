/*
 * Copyright (c) 2011-2013 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2013      Inria. All rights reserved.
 *
 * @precisions normal z -> c d s
 *
 */
#include "dague_internal.h"
#include "dplasma.h"
#include "dplasma/lib/dplasmatypes.h"

#include "zprint.h"

/**
 *******************************************************************************
 *
 * @ingroup dplasma_complex64
 *
 * dplasma_zprint - Print a matrix tile by tile.
 *
 * Rk: This function doesn't use the dplasma_map() function because it has extra
 * control to synchronize the outputs.
 *
 *******************************************************************************
 *
 * @param[in,out] dague
 *          The dague context of the application that will run the operation.
 *
 * @param[in] uplo
 *          Specifies which part of the matrix is printed
 *          = PlasmaUpper: Upper part of A;
 *          = PlasmaLower: Lower part of A;
 *          = PlasmaUpperLower: ALL elements of A.
 *
 * @param[in] A
 *          Descriptor of the distributed matrix A to generate. Any tiled matrix
 *          descriptor can be used.
 *
 *******************************************************************************
 *
 * @return
 *          \retval -i if the ith parameters is incorrect.
 *          \retval 0 on success.
 *
 *******************************************************************************
 *
 * @sa dplasma_cprint
 * @sa dplasma_dprint
 * @sa dplasma_sprint
 *
 ******************************************************************************/
int dplasma_zprint( dague_context_t *dague,
                    PLASMA_enum uplo,
                    const tiled_matrix_desc_t *A)
{
    dague_zprint_object_t* object;

    /* Check input arguments */
    if ((uplo != PlasmaLower) &&
        (uplo != PlasmaUpper) &&
        (uplo != PlasmaUpperLower))
    {
        dplasma_error("dplasma_zplghe", "illegal value of type");
        return -3;
    }

    object = dague_zprint_new( uplo, (dague_ddesc_t*)A);

    if (object != NULL) {
        /* Default type */
        dplasma_add2arena_tile( object->arenas[DAGUE_zprint_DEFAULT_ARENA],
                                A->mb*A->nb*sizeof(dague_complex64_t),
                                DAGUE_ARENA_ALIGNMENT_SSE,
                                MPI_DOUBLE_COMPLEX, A->mb );

        dague_enqueue(dague, (dague_object_t*)object);
        dplasma_progress(dague);

        dplasma_datatype_undefine_type( &(object->arenas[DAGUE_zprint_DEFAULT_ARENA]->opaque_dtt) );
        DAGUE_INTERNAL_OBJECT_DESTRUCT( object );
        return 0;
    }
    else
        return -101;
}
