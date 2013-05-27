/*
 * Copyright (c) 2009-2012 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 *
 * @precisions normal z -> s d c
 *
 */

#include "common.h"
#include "data_dist/matrix/two_dim_rectangle_cyclic.h"

static int check_solution( dague_context_t *dague, int loud,
                           tiled_matrix_desc_t *ddescA,
                           tiled_matrix_desc_t *ddescB,
                           tiled_matrix_desc_t *ddescX );

static int check_inverse( dague_context_t *dague, int loud,
                          tiled_matrix_desc_t *ddescA,
                          tiled_matrix_desc_t *ddescInvA,
                          tiled_matrix_desc_t *ddescI );

static inline int dague_imin(int a, int b) { return (a <= b) ? a : b; };

int main(int argc, char ** argv)
{
    dague_context_t* dague;
    int iparam[IPARAM_SIZEOF];
    int *lu_tab;
    int info = 0;
    int i, ret = 0;
    dplasma_qrtree_t qrtree;
    extern double alpha;

    /* Set defaults for non argv iparams */
    iparam_default_facto(iparam);
    iparam_default_ibnbmb(iparam, 40, 200, 200);
    iparam[IPARAM_LDA] = -'m';
    iparam[IPARAM_LDB] = -'m';

    /* Initialize DAGuE */
    dague = setup_dague(argc, argv, iparam);
    PASTE_CODE_IPARAM_LOCALS(iparam);
    PASTE_CODE_FLOPS(FLOPS_ZGETRF, ((DagDouble_t)M,(DagDouble_t)N));

    LDA = max(M, LDA);

    if ( M != N && check ) {
        fprintf(stderr, "Check is impossible if M != N\n");
        check = 0;
    }

    /* initializing matrix structure */
    PASTE_CODE_ALLOCATE_MATRIX(ddescA, 1,
                               two_dim_block_cyclic, (&ddescA, matrix_ComplexDouble, matrix_Tile,
                                                      nodes, cores, rank, MB, NB, LDA, N, 0, 0,
                                                      M, N, SMB, SNB, P));
    PASTE_CODE_ALLOCATE_MATRIX(ddescTS, 1,
                               two_dim_block_cyclic, (&ddescTS, matrix_ComplexDouble, matrix_Tile,
                                                      nodes, cores, rank, IB, NB, MT*IB, N, 0, 0,
                                                      MT*IB, N, SMB, SNB, P));
    PASTE_CODE_ALLOCATE_MATRIX(ddescTT, 1,
                               two_dim_block_cyclic, (&ddescTT, matrix_ComplexDouble, matrix_Tile,
                                                      nodes, cores, rank, IB, NB, MT*IB, N, 0, 0,
                                                      MT*IB, N, SMB, SNB, P));
    PASTE_CODE_ALLOCATE_MATRIX(ddescIPIV, 1,
                               two_dim_block_cyclic, (&ddescIPIV, matrix_Integer, matrix_Tile,
                                                      nodes, cores, rank, MB, 1, M, NT, 0, 0,
                                                      M, NT, SMB, SNB, P));
    PASTE_CODE_ALLOCATE_MATRIX(ddescA0, check,
                               two_dim_block_cyclic, (&ddescA0, matrix_ComplexDouble, matrix_Tile,
                                                      nodes, cores, rank, MB, NB, LDA, N, 0, 0,
                                                      M, N, SMB, SNB, P));
    /* Random B check */
    PASTE_CODE_ALLOCATE_MATRIX(ddescB, check,
                               two_dim_block_cyclic, (&ddescB, matrix_ComplexDouble, matrix_Tile,
                                                      nodes, cores, rank, MB, NB, LDB, NRHS, 0, 0,
                                                      M, NRHS, SMB, SNB, P));
    PASTE_CODE_ALLOCATE_MATRIX(ddescX, check,
                               two_dim_block_cyclic, (&ddescX, matrix_ComplexDouble, matrix_Tile,
                                                      nodes, cores, rank, MB, NB, LDB, NRHS, 0, 0,
                                                      M, NRHS, SMB, SNB, P));
    /* Inverse check */
    PASTE_CODE_ALLOCATE_MATRIX(ddescInvA, check_inv,
                               two_dim_block_cyclic, (&ddescInvA, matrix_ComplexDouble, matrix_Tile,
                                                      nodes, cores, rank, MB, NB, LDA, N, 0, 0,
                                                      M, N, SMB, SNB, P));
    PASTE_CODE_ALLOCATE_MATRIX(ddescI, check_inv,
                               two_dim_block_cyclic, (&ddescI, matrix_ComplexDouble, matrix_Tile,
                                                      nodes, cores, rank, MB, NB, LDA, N, 0, 0,
                                                      M, N, SMB, SNB, P));

    lu_tab = (int *)malloc( dague_imin(MT, NT)*sizeof(int) );
    for(i=0; i< dague_imin(MT, NT); i++)
        lu_tab[i] = -1;

    /* matrix generation */
    if(loud > 2) printf("+++ Generate matrices ... ");
    dplasma_zplrnt( dague, (tiled_matrix_desc_t *)&ddescA, random_seed);
    dplasma_zlaset( dague, PlasmaUpperLower, 0., 0., (tiled_matrix_desc_t *)&ddescTS);
    dplasma_zlaset( dague, PlasmaUpperLower, 0., 0., (tiled_matrix_desc_t *)&ddescTT);
    dplasma_hqr_init( &qrtree,
                      (tiled_matrix_desc_t *)&ddescA,
                      iparam[IPARAM_LOWLVL_TREE],
                      iparam[IPARAM_HIGHLVL_TREE],
                      iparam[IPARAM_QR_TS_SZE],
                      P,/*iparam[IPARAM_QR_HLVL_SZE],*/
                      0 /*iparam[IPARAM_QR_DOMINO]*/,
                      0 /*iparam[IPARAM_QR_TSRR]  */);
    if ( check ) {
        dplasma_zlacpy( dague, PlasmaUpperLower,
                        (tiled_matrix_desc_t *)&ddescA,
                        (tiled_matrix_desc_t *)&ddescA0 );
        dplasma_zplrnt( dague, (tiled_matrix_desc_t *)&ddescB, random_seed+1 );
        dplasma_zlacpy( dague, PlasmaUpperLower,
                        (tiled_matrix_desc_t *)&ddescB,
                        (tiled_matrix_desc_t *)&ddescX );
    }
    if ( check_inv ) {
        dplasma_zlaset( dague, PlasmaUpperLower, 0., 1., (tiled_matrix_desc_t *)&ddescI);
        dplasma_zlaset( dague, PlasmaUpperLower, 0., 1., (tiled_matrix_desc_t *)&ddescInvA);
    }
    if(loud > 2) printf("Done\n");

    /* Create DAGuE */
    if(loud > 2) printf("+++ Computing getrf_qrf ... ");
    PASTE_CODE_ENQUEUE_KERNEL(dague, zgetrf_qrf,
                              (&qrtree,
                               (tiled_matrix_desc_t*)&ddescA,
                               (tiled_matrix_desc_t*)&ddescIPIV,
                               (tiled_matrix_desc_t*)&ddescTS,
                               (tiled_matrix_desc_t*)&ddescTT,
                               iparam[IPARAM_QR_HLVL_SZE], alpha, lu_tab,
                               &info));
    /* lets rock! */
    PASTE_CODE_PROGRESS_KERNEL(dague, zgetrf_qrf);
    dplasma_zgetrf_qrf_Destruct( DAGUE_zgetrf_qrf );
    if(loud > 2) printf("Done.\n");

    /* Compute percentage of LU/QR */
    {
#if defined(HAVE_MPI)
        {
            int *lu_tab2 = (int*)malloc( MT*sizeof(int) );
            MPI_Allreduce ( lu_tab, lu_tab2, MT, MPI_INT, MPI_MAX, MPI_COMM_WORLD);
            memcpy( lu_tab, lu_tab2, MT*sizeof(int) );
            free(lu_tab2);
        }
#endif

        int i, nblu, nbqr;
        nblu = 0;
        nbqr = dague_imin(MT, NT);
        for(i=0; i<nbqr; i++) {
            nblu += lu_tab[i];
        }
        nbqr -= nblu;

        if (loud > 3 || (rank == 0 && loud)) {
            printf("[%d] LU/QR repartition: %d(%.2f) LU / %d(%.2f) QR \n", rank,
                   nblu, 100. * (double)nblu / (double)(nblu+nbqr),
                   nbqr, 100. * (double)nbqr / (double)(nblu+nbqr));
        }
    }

    if ( info != 0 ) {
        if( rank == 0 && loud ) printf("-- Factorization is suspicious (info = %d) ! \n", info );
        ret |= 1;
    }
    else if ( check ) {
        /*
         * First check with a right hand side
         */
        dplasma_ztrsmpl_qrf( dague, &qrtree,
                             (tiled_matrix_desc_t *)&ddescA,
                             (tiled_matrix_desc_t *)&ddescIPIV,
                             (tiled_matrix_desc_t *)&ddescX,
                             (tiled_matrix_desc_t *)&ddescTS,
                             (tiled_matrix_desc_t *)&ddescTT,
                             lu_tab);
        dplasma_ztrsm(dague, PlasmaLeft, PlasmaUpper, PlasmaNoTrans, PlasmaNonUnit, 1.0,
                      (tiled_matrix_desc_t *)&ddescA,
                      (tiled_matrix_desc_t *)&ddescX);

        /* Check the solution */
        ret |= check_solution( dague, (rank == 0) ? loud : 0,
                               (tiled_matrix_desc_t *)&ddescA0,
                               (tiled_matrix_desc_t *)&ddescB,
                               (tiled_matrix_desc_t *)&ddescX);

        /*
         * Second check with inverse
         */
        if ( check_inv ) {
            dplasma_ztrsmpl_qrf( dague, &qrtree,
                                 (tiled_matrix_desc_t *)&ddescA,
                                 (tiled_matrix_desc_t *)&ddescIPIV,
                                 (tiled_matrix_desc_t *)&ddescInvA,
                                 (tiled_matrix_desc_t *)&ddescTS,
                                 (tiled_matrix_desc_t *)&ddescTT,
                                 lu_tab);
            dplasma_ztrsm(dague, PlasmaLeft, PlasmaUpper, PlasmaNoTrans, PlasmaNonUnit, 1.0,
                          (tiled_matrix_desc_t *)&ddescA,
                          (tiled_matrix_desc_t *)&ddescInvA);

            /* Check the solution */
            ret |= check_inverse(dague, (rank == 0) ? loud : 0,
                                 (tiled_matrix_desc_t *)&ddescA0,
                                 (tiled_matrix_desc_t *)&ddescInvA,
                                 (tiled_matrix_desc_t *)&ddescI);
        }
    }

    if ( check ) {
        dague_data_free(ddescA0.mat);
        dague_ddesc_destroy( (dague_ddesc_t*)&ddescA0);
        dague_data_free(ddescB.mat);
        dague_ddesc_destroy( (dague_ddesc_t*)&ddescB);
        dague_data_free(ddescX.mat);
        dague_ddesc_destroy( (dague_ddesc_t*)&ddescX);
        if ( check_inv ) {
            dague_data_free(ddescInvA.mat);
            dague_ddesc_destroy((dague_ddesc_t*)&ddescInvA);
            dague_data_free(ddescI.mat);
            dague_ddesc_destroy((dague_ddesc_t*)&ddescI);
        }
    }

    dplasma_hqr_finalize( &qrtree );
    cleanup_dague(dague, iparam);

    dague_data_free(ddescA.mat);
    dague_ddesc_destroy((dague_ddesc_t*)&ddescA);
    dague_data_free(ddescTS.mat);
    dague_ddesc_destroy((dague_ddesc_t*)&ddescTS);
    dague_data_free(ddescTT.mat);
    dague_ddesc_destroy((dague_ddesc_t*)&ddescTT);
    dague_data_free(ddescIPIV.mat);
    dague_ddesc_destroy((dague_ddesc_t*)&ddescIPIV);
    free(lu_tab);
    return ret;
}

static int check_solution( dague_context_t *dague, int loud,
                           tiled_matrix_desc_t *ddescA,
                           tiled_matrix_desc_t *ddescB,
                           tiled_matrix_desc_t *ddescX )
{
    int info_solution;
    double Rnorm = 0.0;
    double Anorm = 0.0;
    double Bnorm = 0.0;
    double Xnorm, result;
    int m = ddescB->m;
    double eps = LAPACKE_dlamch_work('e');

    Anorm = dplasma_zlange(dague, PlasmaInfNorm, ddescA);
    Bnorm = dplasma_zlange(dague, PlasmaInfNorm, ddescB);
    Xnorm = dplasma_zlange(dague, PlasmaInfNorm, ddescX);

    /* Compute b - A*x */
    dplasma_zgemm( dague, PlasmaNoTrans, PlasmaNoTrans, -1.0, ddescA, ddescX, 1.0, ddescB);

    Rnorm = dplasma_zlange(dague, PlasmaInfNorm, ddescB);

    result = Rnorm / ( ( Anorm * Xnorm + Bnorm ) * m * eps ) ;

    if ( loud > 2 ) {
        printf("============\n");
        printf("Checking the Residual of the solution \n");
        if ( loud > 3 )
            printf( "-- ||A||_oo = %e, ||X||_oo = %e, ||B||_oo= %e, ||A X - B||_oo = %e\n",
                    Anorm, Xnorm, Bnorm, Rnorm );
        printf("-- ||Ax-B||_oo/((||A||_oo||x||_oo+||B||_oo).N.eps) = %e \n", result);
    }

    if (  isnan(Xnorm) || isinf(Xnorm) || isnan(result) || isinf(result) || (result > 60.0) ) {
        if( loud ) printf("-- Solution with b is suspicious ! \n");
        info_solution = 1;
    }
    else{
        if( loud ) printf("-- Solution with b is CORRECT ! \n");
        info_solution = 0;
    }

    return info_solution;
}

static int check_inverse( dague_context_t *dague, int loud,
                          tiled_matrix_desc_t *ddescA,
                          tiled_matrix_desc_t *ddescInvA,
                          tiled_matrix_desc_t *ddescI )
{
    int info_solution;
    double Anorm    = 0.0;
    double InvAnorm = 0.0;
    double Rnorm, result;
    int m = ddescA->m;
    double eps = LAPACKE_dlamch_work('e');

    Anorm    = dplasma_zlange(dague, PlasmaInfNorm, ddescA   );
    InvAnorm = dplasma_zlange(dague, PlasmaInfNorm, ddescInvA);

    /* Compute I - A*A^{-1} */
    dplasma_zgemm( dague, PlasmaNoTrans, PlasmaNoTrans, -1.0, ddescA, ddescInvA, 1.0, ddescI);

    Rnorm = dplasma_zlange(dague, PlasmaInfNorm, ddescI);

    result = Rnorm / ( ( Anorm * InvAnorm ) * m * eps ) ;

    if ( loud > 2 ) {
        printf("============\n");
        printf("Checking the Residual of the solution \n");
        if ( loud > 3 )
            printf( "-- ||A||_oo = %e, ||A^{-1}||_oo = %e, ||A A^{-1} - I||_oo = %e\n",
                    Anorm, InvAnorm, Rnorm );
        printf("-- ||AA^{-1}-I||_oo/((||A||_oo||A^{-1}||_oo).N.eps) = %e \n", result);
    }

    if (  isnan(Rnorm) || isinf(Rnorm) || isnan(result) || isinf(result) || (result > 60.0) ) {
        if( loud ) printf("-- Solution with I is suspicious ! \n");
        info_solution = 1;
    }
    else{
        if( loud ) printf("-- Solution with I is CORRECT ! \n");
        info_solution = 0;
    }

    return info_solution;
}
