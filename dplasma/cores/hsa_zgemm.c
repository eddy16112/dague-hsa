/*
 * Copyright (c) 2010-2014 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 *
 * @precisions normal z -> z c d s
 *
 */
#include <dague_config.h>
#include <stdlib.h>
#include <core_blas.h>
#include <gpuKernels.h>

#include "dague.h"
#include "dague/execution_unit.h"
#include "dague/class/fifo.h"
#include "data_dist/matrix/matrix.h"
#include "dague/data_internal.h"
#include "hsa_zgemm.h"

#define flow_A  1
#define flow_B  2
#define flow_C  0

#define KERNEL_NAME zgemm

static inline
int hsa_kernel_submit_zgemm( hsa_device_t* hsa_device,
                           dague_hsa_context_t* this_task,
                           dague_hsa_exec_stream_t* hsa_stream);

typedef struct dague_hsa_zgemm_args_s {
    dague_hsa_context_t super;
    dague_complex64_t alpha, beta;
    PLASMA_enum transA, transB;
    int M, N, K;
    int lda, ldb, ldc;
    int m, n, k;
} dague_hsa_zgemm_args_t;


#include <dague/devices/hsa/hsa_scheduling.h>

static inline int
hsa_kernel_submit_zgemm( hsa_device_t        *hsa_device,
                         dague_hsa_context_t *hsa_task,
                         dague_hsa_exec_stream_t* hsa_stream )
{
    dague_execution_context_t *this_task = hsa_task->ec;
    dague_hsa_zgemm_args_t *args = (dague_hsa_zgemm_args_t*)hsa_task;
    
    dague_data_copy_t *gC = this_task->data[flow_C].data_in;
    void *C = DAGUE_DATA_COPY_GET_PTR(gC);
    dague_data_copy_t *gA = this_task->data[flow_A].data_in;
    void *A = DAGUE_DATA_COPY_GET_PTR(gA); 
    dague_data_copy_t *gB = this_task->data[flow_B].data_in;
    void *B = DAGUE_DATA_COPY_GET_PTR(gB);
 
    SNK_INIT_LPARM(lparm,1);
    int grid_size = args->N/8;
    lparm[0].ndim = 2;
    lparm[0].gdims[0] = grid_size;
    lparm[0].gdims[1] = grid_size;
    lparm[0].ldims[0] = 8;
    lparm[0].ldims[1] = 8;
    lparm[0].stream = -1;
    lparm[0].barrier = SNK_UNORDERED;
    lparm[0].acquire_fence_scope = 2;
    lparm[0].release_fence_scope = 2;
 
    zgemmBlock(args->M, args->N, args->K, args->alpha, args->beta, A, B, C, args->lda, args->ldb, args->ldc, 0, 0, 0, lparm);
    
    printf("hsa_zgemm( %d, %d, %d )\n", args->m, args->n, args->k);


    return 0;
}

int hsa_zgemm( dague_execution_unit_t* eu_context,
               dague_execution_context_t* this_task,
               PLASMA_enum transA, PLASMA_enum transB,
               int M, int N, int K,
               dague_complex64_t alpha, int lda,
                                        int ldb,
               dague_complex64_t beta,  int ldc,
               int m, int n, int k )
{
    dague_hsa_zgemm_args_t *hsa_task;
    hsa_task = (dague_hsa_zgemm_args_t*)malloc(sizeof(dague_hsa_zgemm_args_t));
    OBJ_CONSTRUCT(hsa_task, dague_list_item_t);
    hsa_task->super.ec = this_task;
    hsa_task->super.task_type = 0;
    hsa_task->alpha    = alpha;
    hsa_task->beta     = beta;
    hsa_task->transA   = transA;
    hsa_task->transB   = transB;
    hsa_task->M        = M;
    hsa_task->N        = N;
    hsa_task->K        = K;
    hsa_task->lda      = lda;
    hsa_task->ldb      = ldb;
    hsa_task->ldc      = ldc;
    hsa_task->m        = m;
    hsa_task->n        = n;
    hsa_task->k        = k;
    printf("task %p created for gemm (%d, %d, %d)\n", hsa_task, m, n, k);
    return hsa_kernel_scheduler_zgemm( eu_context, (dague_hsa_context_t*)hsa_task, 1 );
/*
    dague_data_copy_t *gC = this_task->data[flow_C].data_in;
    void *C = DAGUE_DATA_COPY_GET_PTR(gC); (void)C;
    dague_data_copy_t *gA = this_task->data[flow_A].data_in;
    void *A = DAGUE_DATA_COPY_GET_PTR(gA); (void)A;
    dague_data_copy_t *gB = this_task->data[flow_B].data_in;
    void *B = DAGUE_DATA_COPY_GET_PTR(gB); (void)B;

    SNK_INIT_LPARM(lparm,1);
    int grid_size = N/8;
    lparm[0].ndim = 2;
    lparm[0].gdims[0] = grid_size;
    lparm[0].gdims[1] = grid_size;
    lparm[0].ldims[0] = 8;
    lparm[0].ldims[1] = 8;
    lparm[0].stream = -1;
    lparm[0].barrier = SNK_UNORDERED;
    lparm[0].acquire_fence_scope = 2;
    lparm[0].release_fence_scope = 2;

    zgemmBlock(M, N, K, alpha, beta, A, B, C, lda, ldb, ldc, 0, 0, 0, lparm);

    return DAGUE_HOOK_RETURN_ASYNC;
*/
}
