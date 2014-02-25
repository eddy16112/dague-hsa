/*
 * Copyright (c) 2010-2013 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2013      Inria. All rights reserved.
 * $COPYRIGHT
 *
 * @precisions normal z -> s d c
 *
 */

#include "dplasma.h"

/**
 *******************************************************************************
 *
 * @ingroup dplasma_complex64
 *
 * dplasma_zgeqrs - Computes a minimum-norm solution min || A*X - B || using the
 * QR factorization A = Q*R computed by dplasma_zgeqrf().
 *
 *******************************************************************************
 *
 * @param[in,out] dague
 *          The dague context of the application that will run the operation.
 *
 * @param[in] A
 *          Descriptor of the matrix A of size M-by-N factorized with the
 *          dplasma_zgeqrf_New() routine.
 *          On entry, the i-th column must contain the vector which
 *          defines the elementary reflector H(i), for i = 1,2,...,k, as
 *          returned by dplasma_zgeqrf_New() in the first k columns of its array
 *          argument A.
 *
 * @param[in] T
 *          Descriptor of the matrix T distributed exactly as the A matrix. T.mb
 *          defines the IB parameter of tile QR algorithm. This matrix must be
 *          of size A.mt * T.mb - by - A.nt * T.nb, with T.nb == A.nb.
 *          This matrix is initialized during the call to dplasma_zgeqrf_New().
 *
 * @param[in,out] B
 *          Descriptor that covers both matrix B and X.
 *          On entry, the M-by-NRHS right hand side matrix B.
 *          On exit, the N-by-NRHS solution matrix X.
 *          M >= N >= 0.
 *
 *******************************************************************************
 *
 * @return
 *          \retval -i if the ith parameters is incorrect.
 *          \retval 0 on success.
 *
 *******************************************************************************
 *
 * @sa dplasma_cgeqrs
 * @sa dplasma_dgeqrs
 * @sa dplasma_sgeqrs
 *
 ******************************************************************************/
int
dplasma_zgeqrs( dague_context_t *dague,
                tiled_matrix_desc_t* A,
                tiled_matrix_desc_t* T,
                tiled_matrix_desc_t* B )
{
    tiled_matrix_desc_t *subA;
    tiled_matrix_desc_t *subB;

    /* Check input arguments */
    if ( A->n > A->m ) {
        dplasma_error("dplasma_zgeqrs", "illegal dimension of A, A->n > A->m");
        return -1;
    }
    if ( (T->nt != A->nt) || (T->mt != A->mt) ) {
        dplasma_error("dplasma_zgeqrs", "illegal size of T (T should have as many tiles as A)");
        return -2;
    }
    if ( B->m < A->m ) {
        dplasma_error("dplasma_zgeqrs", "illegal dimension of B, (B->m < A->m)");
        return -3;
    }

    subA = tiled_matrix_submatrix( A, 0, 0, A->n, A->n );
    subB = tiled_matrix_submatrix( B, 0, 0, A->n, B->n );

#ifdef DAGUE_COMPOSITION

    dague_handle_t *dague_zunmqr = NULL;
    dague_handle_t *dague_ztrsm  = NULL;

    dague_zunmqr = dplasma_zunmqr_New( PlasmaLeft, PlasmaConjTrans, A, T, B );
    dague_ztrsm  = dplasma_ztrsm_New(  PlasmaLeft, PlasmaUpper, PlasmaNoTrans, PlasmaNonUnit, 1.0, subA, subB );

    dague_enqueue( dague, dague_zunmqr );
    dague_enqueue( dague, dague_ztrsm );

    dplasma_progress( dague );

    dplasma_zunmqr_Destruct( dague_zunmqr );
    dplasma_ztrsm_Destruct( dague_ztrsm );

#else

    dplasma_zunmqr( dague, PlasmaLeft, PlasmaConjTrans, A, T, B );
    dplasma_ztrsm(  dague, PlasmaLeft, PlasmaUpper, PlasmaNoTrans, PlasmaNonUnit, 1.0, subA, subB );

#endif

    free(subA);
    free(subB);

    return 0;
}
