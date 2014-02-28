/*
 * Copyright (c) 2010-2012 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 *
 * @precisions normal z -> s d c
 *
 */
#include "dague_internal.h"
#include <core_blas.h>
#include "dplasma.h"

/**
 *******************************************************************************
 *
 * @ingroup dplasma_complex64
 *
 * dplasma_zgetrs - Solves a system of linear equations A * X = B with a general
 * square matrix A using the LU factorization with incremental pivoting strategy
 * computed by dplasma_zgetrf_incpiv().
 *
 *******************************************************************************
 *
 * @param[in,out] dague
 *          The dague context of the application that will run the operation.
 *
 * @param[in] trans
 *          Specifies whether the matrix A is transposed, not transposed or
 *          conjugate transposed:
 *          = PlasmaNoTrans:   A is transposed;
 *          = PlasmaTrans:     A is not transposed;
 *          = PlasmaConjTrans: A is conjugate transposed.
 *          Currently only PlasmaNoTrans is supported.
 *
 * @param[in] A
 *          Descriptor of the distributed matrix A to be factorized.
 *          On entry, The factorized matrix through dplasma_zgetrf_incpiv_New()
 *          routine.  Elements on and above the diagonal are the elements of
 *          U. Elements belowe the diagonal are NOT the classic L, but the L
 *          factors obtaines by succesive pivoting.
 *
 * @param[in] L
 *          Descriptor of the matrix L distributed exactly as the A matrix.
 *           - If IPIV != NULL, L.mb defines the IB parameter of the tile LU
 *          algorithm. This matrix must be of size A.mt * L.mb - by - A.nt *
 *          L.nb, with L.nb == A.nb.
 *          On entry, contains auxiliary information required to solve the
 *          system and generated by dplasma_zgetrf_inciv_New().
 *           - If IPIV == NULL, pivoting information are stored within
 *          L. (L.mb-1) defines the IB parameter of the tile LU algorithm. This
 *          matrix must be of size A.mt * L.mb - by - A.nt * L.nb, with L.nb =
 *          A.nb, and L.mb = ib+1.
 *          The first A.mb elements contains the IPIV information, the leftover
 *          contains auxiliary information required to solve the system.
 *
 * @param[in] IPIV
 *          Descriptor of the IPIV matrix. Should be distributed exactly as the
 *          A matrix. This matrix must be of size A.m - by - A.nt with IPIV.mb =
 *          A.mb and IPIV.nb = 1.
 *          On entry, contains the pivot indices of the successive row
 *          interchanged performed during the factorization.
 *          If IPIV == NULL, rows interchange information is stored within L.
 *
 * @param[in,out] B
 *          On entry, the N-by-NRHS right hand side matrix B.
 *          On exit, if return value = 0, B is overwritten by the solution matrix X.
 *
 *******************************************************************************
 *
 * @return
 *          \retval -i if the ith parameters is incorrect.
 *          \retval 0 on success.
 *
 *******************************************************************************
 *
 * @sa dplasma_zgetrs_New
 * @sa dplasma_zgetrs_Destruct
 * @sa dplasma_cgetrs
 * @sa dplasma_dgetrs
 * @sa dplasma_sgetrs
 *
 ******************************************************************************/
int
dplasma_zgetrs_incpiv(dague_context_t *dague,
                      PLASMA_enum trans,
                      tiled_matrix_desc_t *A,
                      tiled_matrix_desc_t *L,
                      tiled_matrix_desc_t *IPIV,
                      tiled_matrix_desc_t *B)
{
    /* Check input arguments */
    if (trans != PlasmaNoTrans) {
        dplasma_error("dplasma_zgetrs", "only PlasmaNoTrans supported");
        return -1;
    }

#ifdef DAGUE_COMPOSITION
    dague_object_t *dague_ztrsmpl = NULL;
    dague_object_t *dague_ztrsm   = NULL;

    dague_ztrsmpl = dplasma_ztrsmpl_New(A, L, IPIV, B);
    dague_ztrsm   = dplasma_ztrsm_New(PlasmaLeft, PlasmaUpper, PlasmaNoTrans, PlasmaNonUnit, 1.0, A, B);

    dague_enqueue( dague, dague_ztrsmpl );
    dague_enqueue( dague, dague_ztrsm   );

    dplasma_progress( dague );

    dplasma_ztrsm_Destruct( dague_ztrsmpl );
    dplasma_ztrsm_Destruct( dague_ztrsm   );
#else
    dplasma_ztrsmpl(dague, A, L, IPIV, B );
    dplasma_ztrsm( dague, PlasmaLeft, PlasmaUpper, PlasmaNoTrans, PlasmaNonUnit, 1.0, A, B );
#endif
    return 0;
}

