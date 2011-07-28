/*
 * Copyright (c) 2009-2011 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 *
 * @precisions normal z -> s d c
 *
 */

#include "common.h"
#include "data_dist/matrix/two_dim_rectangle_cyclic.h"

#define DO_UBLE do##uble

#define FMULS_GEQRF(M, N) (((M) > (N)) ? ((N) * ((N) * (  0.5-(1./3.) * (N) + (M)) + (M))) \
                                       : ((M) * ((M) * ( -0.5-(1./3.) * (M) + (N)) + 2.*(N))))
#define FADDS_GEQRF(M, N) (((M) > (N)) ? ((N) * ((N) * (  0.5-(1./3.) * (N) + (M)))) \
                                       : ((M) * ((M) * ( -0.5-(1./3.) * (M) + (N)) + (N))))

static int check_orthogonality(dague_context_t *dague, tiled_matrix_desc_t *Q);
static int check_factorization(dague_context_t *dague, tiled_matrix_desc_t *Aorig, tiled_matrix_desc_t *A, tiled_matrix_desc_t *Q);

int main(int argc, char ** argv)
{
    dague_context_t* dague;
    int iparam[IPARAM_SIZEOF];

    /* Set defaults for non argv iparams */
    iparam_default_facto(iparam);
    iparam_default_ibnbmb(iparam, 48, 144, 144);
    iparam[IPARAM_LDA] = -'m';
    iparam[IPARAM_LDB] = -'m';

    /* Initialize DAGuE */
    dague = setup_dague(argc, argv, iparam);
    PASTE_CODE_IPARAM_LOCALS(iparam)
    PASTE_CODE_FLOPS_COUNT(FADDS_GEQRF, FMULS_GEQRF, ((DagDouble_t)M,(DagDouble_t)N))
      
    LDA = max(M, LDA);
    /* initializing matrix structure */
    PASTE_CODE_ALLOCATE_MATRIX(ddescA, 1, 
        two_dim_block_cyclic, (&ddescA, matrix_ComplexDouble, 
                               nodes, cores, rank, MB, NB, LDA, N, 0, 0, 
                               M, N, SMB, SNB, P));
    PASTE_CODE_ALLOCATE_MATRIX(ddescTS, 1, 
        two_dim_block_cyclic, (&ddescTS, matrix_ComplexDouble, 
                               nodes, cores, rank, IB, NB, MT*IB, N, 0, 0, 
                               MT*IB, N, SMB, SNB, P));
    PASTE_CODE_ALLOCATE_MATRIX(ddescTT, 1, 
        two_dim_block_cyclic, (&ddescTT, matrix_ComplexDouble, 
                               nodes, cores, rank, IB, NB, MT*IB, N, 0, 0, 
                               MT*IB, N, SMB, SNB, P));
    PASTE_CODE_ALLOCATE_MATRIX(ddescA0, check, 
        two_dim_block_cyclic, (&ddescA0, matrix_ComplexDouble, 
                               nodes, cores, rank, MB, NB, LDA, N, 0, 0, 
                               M, N, SMB, SNB, P));
    PASTE_CODE_ALLOCATE_MATRIX(ddescQ, check, 
        two_dim_block_cyclic, (&ddescQ, matrix_ComplexDouble, 
                               nodes, cores, rank, MB, NB, LDA, N, 0, 0, 
                               M, N, SMB, SNB, P));


#if defined(DAGUE_PROF_TRACE)
    ddescA.super.super.key = strdup("A");
    ddescTS.super.super.key = strdup("TS");
    ddescTT.super.super.key = strdup("TT");
    ddescA0.super.super.key = strdup("A0");
    ddescQ.super.super.key = strdup("Q");
#endif

    if(simul) {
        /* Cannot use double... Use DO_UBLE... */
        DO_UBLE tasks_costs[6];
        dague_execution_context_t *init;
        dague_object_t *o;
        DO_UBLE cpath;
        
        /* Indices of the tasks_costs are to be found in the generated c file (look for <basename>_functions[]) */
        tasks_costs[0] = 6.0; /**< zttmqr */
        tasks_costs[1] = 0.0; /**< zttmqr_out */
        tasks_costs[2] = 2.0; /**< zttqrt */
        tasks_costs[3] = 0.0; /**< zttqrt_out */
        tasks_costs[4] = 4.0; /**< sormqr */
        tasks_costs[5] = 4.0; /**< zgeqrt */

        dplasma_zplrnt( dague, (tiled_matrix_desc_t *)&ddescA, 3872);
        dplasma_zlaset( dague, PlasmaUpperLower, 0., 0., (tiled_matrix_desc_t *)&ddescTS);
        dplasma_zlaset( dague, PlasmaUpperLower, 0., 0., (tiled_matrix_desc_t *)&ddescTT);

        o = dplasma_zgeqrf_param_New(iparam[IPARAM_LOWLVL_TREE], iparam[IPARAM_HIGHLVL_TREE],
                                     iparam[IPARAM_QR_TS_SZE], iparam[IPARAM_QR_HLVL_SZE],
                                     (tiled_matrix_desc_t*)&ddescA,
                                     (tiled_matrix_desc_t*)&ddescTS,
                                     (tiled_matrix_desc_t*)&ddescTT);
        init = NULL;
        o->startup_hook( dague, o, &init );
        
        /* We use a single cost of 4.0 for all communications */
        cpath = dague_compute_critical_path( dague->execution_units[0],
                                             init,
                                             tasks_costs,
                                             4.0,
                                             3 );
        
        printf("Critical Path Length: %g\n", cpath);
    } else {

        /* matrix generation */
        if(loud > 2) printf("+++ Generate matrices ... ");
        dplasma_zplrnt( dague, (tiled_matrix_desc_t *)&ddescA, 3872);
        if( check )
            dplasma_zlacpy( dague, PlasmaUpperLower,
                            (tiled_matrix_desc_t *)&ddescA, (tiled_matrix_desc_t *)&ddescA0 );
        dplasma_zlaset( dague, PlasmaUpperLower, 0., 0., (tiled_matrix_desc_t *)&ddescTS);
        dplasma_zlaset( dague, PlasmaUpperLower, 0., 0., (tiled_matrix_desc_t *)&ddescTT);
        if(loud > 2) printf("Done\n");
      
        /* Create DAGuE */
        PASTE_CODE_ENQUEUE_KERNEL(dague, zgeqrf_param,
                                  (iparam[IPARAM_LOWLVL_TREE], iparam[IPARAM_HIGHLVL_TREE],
                                   iparam[IPARAM_QR_TS_SZE], iparam[IPARAM_QR_HLVL_SZE],
                                   (tiled_matrix_desc_t*)&ddescA,
                                   (tiled_matrix_desc_t*)&ddescTS,
                                   (tiled_matrix_desc_t*)&ddescTT));
      
        /* lets rock! */
        PASTE_CODE_PROGRESS_KERNEL(dague, zgeqrf_param);
        dplasma_zgeqrf_param_Destruct( DAGUE_zgeqrf_param );

        if( check ) {
            int info_ortho, info_facto;
          
            if(loud > 2) fprintf(stderr, "+++ Generate the Q ...");
            dplasma_zlaset( dague, PlasmaUpperLower, 0., 1., (tiled_matrix_desc_t *)&ddescQ);
            dplasma_zungqr_param( dague, 
                                  iparam[IPARAM_LOWLVL_TREE], iparam[IPARAM_HIGHLVL_TREE],
                                  iparam[IPARAM_QR_TS_SZE], iparam[IPARAM_QR_HLVL_SZE],
                                  (tiled_matrix_desc_t *)&ddescA, 
                                  (tiled_matrix_desc_t *)&ddescTS, 
                                  (tiled_matrix_desc_t *)&ddescTT, 
                                  (tiled_matrix_desc_t *)&ddescQ);
            if(loud > 2) fprintf(stderr, "Done\n");
          
            /* Check the orthogonality, factorization and the solution */
            info_ortho = check_orthogonality(dague, (tiled_matrix_desc_t *)&ddescQ);
            info_facto = check_factorization(dague, (tiled_matrix_desc_t *)&ddescA0, 
                                             (tiled_matrix_desc_t *)&ddescA, 
                                             (tiled_matrix_desc_t *)&ddescQ);
          
          
            dague_data_free(ddescA0.mat);
            dague_data_free(ddescQ.mat);
            dague_ddesc_destroy((dague_ddesc_t*)&ddescA0);
            dague_ddesc_destroy((dague_ddesc_t*)&ddescQ);
        }
    }
    
    dague_data_free(ddescA.mat);
    dague_data_free(ddescTS.mat);
    dague_data_free(ddescTT.mat);
    dague_ddesc_destroy((dague_ddesc_t*)&ddescA);
    dague_ddesc_destroy((dague_ddesc_t*)&ddescTS);
    dague_ddesc_destroy((dague_ddesc_t*)&ddescTT);
    
    cleanup_dague(dague, iparam);
    
    return EXIT_SUCCESS;
}

/*-------------------------------------------------------------------
 * Check the orthogonality of Q
 */
static int check_orthogonality(dague_context_t *dague, tiled_matrix_desc_t *Q)
{
    two_dim_block_cyclic_t *twodQ = (two_dim_block_cyclic_t *)Q;
    double normQ = 999999.0;
    double result;
    double eps = LAPACKE_dlamch_work('e');
    int info_ortho;
    int M = Q->m;
    int N = Q->n;
    int minMN = min(M, N);

    PASTE_CODE_ALLOCATE_MATRIX(Id, 1, 
        two_dim_block_cyclic, (&Id, matrix_ComplexDouble, 
                               Q->super.nodes, Q->super.cores, twodQ->grid.rank, 
                               Q->mb, Q->nb, minMN, minMN, 0, 0, 
                               minMN, minMN, twodQ->grid.strows, twodQ->grid.stcols, twodQ->grid.rows));

    dplasma_zlaset( dague, PlasmaUpperLower, 0., 1., (tiled_matrix_desc_t *)&Id);

    /* Perform Id - Q'Q (could be done with Herk) */
    if ( M >= N ) {
      dplasma_zgemm( dague, PlasmaConjTrans, PlasmaNoTrans, 
                     1.0, Q, Q, -1.0, (tiled_matrix_desc_t*)&Id );
    } else {
      dplasma_zgemm( dague, PlasmaNoTrans, PlasmaConjTrans, 
                     1.0, Q, Q, -1.0, (tiled_matrix_desc_t*)&Id );
    }

    normQ = dplasma_zlange(dague, PlasmaMaxNorm, (tiled_matrix_desc_t*)&Id);

    result = normQ / (minMN * eps);
    printf("============\n");
    printf("Checking the orthogonality of Q \n");
    printf("||Id-Q'*Q||_oo / (N*eps) = %e \n", result);

    if ( isnan(result) || isinf(result) || (result > 60.0) ) {
        printf("-- Orthogonality is suspicious ! \n");
        info_ortho=1;
    }
    else {
        printf("-- Orthogonality is CORRECT ! \n");
        info_ortho=0;
    }

    dague_data_free(Id.mat);
    dague_ddesc_destroy((dague_ddesc_t*)&Id);
    return info_ortho;
}

/*-------------------------------------------------------------------
 * Check the orthogonality of Q
 */

static int check_factorization(dague_context_t *dague, tiled_matrix_desc_t *Aorig, tiled_matrix_desc_t *A, tiled_matrix_desc_t *Q)
{
    two_dim_block_cyclic_t *twodA = (two_dim_block_cyclic_t *)A;
    double Anorm, Rnorm;
    double result;
    double eps = LAPACKE_dlamch_work('e');
    int info_factorization;
    int M = A->m;
    int N = A->n;
    int minMN = min(M, N);

    PASTE_CODE_ALLOCATE_MATRIX(Residual, 1, 
        two_dim_block_cyclic, (&Residual, matrix_ComplexDouble, 
                               A->super.nodes, A->super.cores, twodA->grid.rank, 
                               A->mb, A->nb, M, N, 0, 0, 
                               M, N, twodA->grid.strows, twodA->grid.stcols, twodA->grid.rows));

    PASTE_CODE_ALLOCATE_MATRIX(RL, 1, 
        two_dim_block_cyclic, (&RL, matrix_ComplexDouble, 
                               A->super.nodes, A->super.cores, twodA->grid.rank, 
                               A->mb, A->nb, minMN, minMN, 0, 0, 
                               minMN, minMN, twodA->grid.strows, twodA->grid.stcols, twodA->grid.rows));

    /* Extract the L */
    dplasma_zlacpy( dague, PlasmaUpperLower, Aorig, (tiled_matrix_desc_t *)&Residual );

    dplasma_zlaset( dague, PlasmaUpperLower, 0., 0., (tiled_matrix_desc_t *)&RL);
    if (M >= N) {
        /* Extract the R */
        dplasma_zlacpy( dague, PlasmaUpper, A, (tiled_matrix_desc_t *)&RL );
        
        /* Perform Residual = Aorig - Q*R */
        dplasma_zgemm( dague, PlasmaNoTrans, PlasmaNoTrans, 
                       -1.0, Q, (tiled_matrix_desc_t *)&RL, 
                       1.0, (tiled_matrix_desc_t *)&Residual);
    } else {
        /* Extract the L */
        dplasma_zlacpy( dague, PlasmaLower, A, (tiled_matrix_desc_t *)&RL );
        
        /* Perform Residual = Aorig - L*Q */
        dplasma_zgemm( dague, PlasmaNoTrans, PlasmaNoTrans, 
                       -1.0, (tiled_matrix_desc_t *)&RL, Q, 
                       1.0, (tiled_matrix_desc_t *)&Residual);
    }
    
    /* Free RL */
    dague_data_free(RL.mat);
    dague_ddesc_destroy((dague_ddesc_t*)&RL);
    
    Rnorm = dplasma_zlange(dague, PlasmaMaxNorm, (tiled_matrix_desc_t*)&Residual);
    Anorm = dplasma_zlange(dague, PlasmaMaxNorm, Aorig);

    result = Rnorm / ( Anorm * minMN * eps);

    if (M >= N) {
        printf("============\n");
        printf("Checking the QR Factorization \n");
        printf("-- ||A-QR||_oo/(||A||_oo.N.eps) = %e \n", result );
    }
    else {
        printf("============\n");
        printf("Checking the LQ Factorization \n");
        printf("-- ||A-LQ||_oo/(||A||_oo.N.eps) = %e \n", result );
    }

    if ( isnan(result) || isinf(result) || (result > 60.0) ) {
        printf("-- Factorization is suspicious ! \n");
        info_factorization = 1;
    }
    else {
        printf("-- Factorization is CORRECT ! \n");
        info_factorization = 0;
    }

    dague_data_free(Residual.mat);
    dague_ddesc_destroy((dague_ddesc_t*)&Residual);
    return info_factorization;
}
