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
#include "dague_internal.h"
#include "dplasma.h"

/**
 *******************************************************************************
 *
 * @ingroup dplasma_complex64_t
 *
 * dplasma_zgelqs_param - Computes a minimum-norm solution min || A*X - B || using the
 * LQ factorization A = L*Q computed by dplasma_zgelqf().
 *
 *******************************************************************************
 *
 * @param[in,out] dague
 *          The dague context of the application that will run the operation.
 *
 * @param[in] qrtree
 *          The structure that describes the trees used to perform the
 *          hierarchical QR factorization.
 *          See dplasma_hqr_init() or dplasma_systolic_init().
 *
 * @param[in] A
 *          Descriptor of the matrix A of size M-by-N factorized with the
 *          dplasma_zgelqf_New() routine.
 *          On entry, the i-th row must contain the vector which
 *          defines the elementary reflector H(i), for i = 1,2,...,k, as
 *          returned by dplasma_zgelqf_New() in the first k rows of its array
 *          argument A.
 *
 * @param[in] TS
 *          Descriptor of the matrix TS distributed exactly as the A
 *          matrix. TS.mb defines the IB parameter of tile QR algorithm. This
 *          matrix must be of size A.mt * TS.mb - by - A.nt * TS.nb, with TS.nb
 *          == A.nb.  This matrix is initialized during the call to
 *          dplasma_zgelqf_param_New().
 *
 * @param[in] TT
 *          Descriptor of the matrix TT distributed exactly as the A
 *          matrix. TT.mb defines the IB parameter of tile QR algorithm. This
 *          matrix must be of size A.mt * TT.mb - by - A.nt * TT.nb, with TT.nb
 *          == A.nb.  This matrix is initialized during the call to
 *          dplasma_zgelqf_param_New().
 *
 * @param[in,out] B
 *          Descriptor that covers both matrix B and X.
 *          On entry, the N-by-NRHS right hand side matrix B.
 *          On exit, the M-by-NRHS solution matrix X.
 *          N >= M >= 0.
 *
 *******************************************************************************
 *
 * @return
 *          \retval -i if the ith parameters is incorrect.
 *          \retval 0 on success.
 *
 *******************************************************************************
 *
 * @sa dplasma_cgelqs
 * @sa dplasma_dgelqs
 * @sa dplasma_sgelqs
 *
 ******************************************************************************/
int
dplasma_zgelqs_param( dague_context_t *dague,
                      dplasma_qrtree_t *qrtree,
                      tiled_matrix_desc_t* A,
                      tiled_matrix_desc_t* TS,
                      tiled_matrix_desc_t* TT,
                      tiled_matrix_desc_t* B )
{
    tiled_matrix_desc_t *subA;
    tiled_matrix_desc_t *subB;

    /* Check input arguments */
    if ( A->m > A->n ) {
        dplasma_error("dplasma_zgelqs_param", "illegal dimension of A, A->n > A->m");
        return -1;
    }
    if ( (TS->nt != A->nt) || (TS->mt != A->mt) ) {
        dplasma_error("dplasma_zgelqs_param", "illegal size of TS (TS should have as many tiles as A)");
        return -2;
    }
    if ( (TT->nt != A->nt) || (TT->mt != A->mt) ) {
        dplasma_error("dplasma_zgelqs_param", "illegal size of TT (TT should have as many tiles as A)");
        return -2;
    }
    if ( B->m < A->n ) {
        dplasma_error("dplasma_zgelqs_param", "illegal dimension of B, (B->m < A->n)");
        return -3;
    }

    subA = tiled_matrix_submatrix( A, 0, 0, A->m, A->m );
    subB = tiled_matrix_submatrix( B, 0, 0, A->m, B->n );

#ifdef DAGUE_COMPOSITION

    dague_object_t *dague_zunmlq = NULL;
    dague_object_t *dague_ztrsm  = NULL;

    dague_ztrsm  = dplasma_ztrsm_New(  PlasmaLeft, PlasmaLower, PlasmaNoTrans, PlasmaNonUnit, 1.0, subA, subB );
    dague_zunmlq = dplasma_zunmlq_param_New( PlasmaLeft, PlasmaConjTrans, qrtree, A, TS, TT, B );

    dague_enqueue( dague, dague_ztrsm );
    dague_enqueue( dague, dague_zunmlq );

    dplasma_progress( dague );

    dplasma_ztrsm_Destruct( dague_ztrsm );
    dplasma_ztrsm_Destruct( dague_zunmlq );

#else

    dplasma_ztrsm(  dague, PlasmaLeft, PlasmaLower, PlasmaNoTrans, PlasmaNonUnit, 1.0, subA, subB );
    dplasma_zunmlq_param( dague, PlasmaLeft, PlasmaConjTrans, qrtree, A, TS, TT, B );

#endif

    free(subA);
    free(subB);

    return 0;
}
