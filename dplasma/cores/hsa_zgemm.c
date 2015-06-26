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

int hsa_zgemm( dague_execution_unit_t* eu_context,
               dague_execution_context_t* this_task,
               PLASMA_enum transA, PLASMA_enum transB,
               int M, int N, int K,
               dague_complex64_t alpha, int lda,
                                        int ldb,
               dague_complex64_t beta,  int ldc )
{
    dague_data_copy_t *gC = this_task->data[0].data_in;
    void *C = DAGUE_DATA_COPY_GET_PTR(gC); (void)C;
    dague_data_copy_t *gA = this_task->data[1].data_in;
    void *A = DAGUE_DATA_COPY_GET_PTR(gA); (void)A;
    dague_data_copy_t *gB = this_task->data[2].data_in;
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

    zgemmBlock(M, N, K, alpha, 1.0, A, B, C, lda, ldb, ldc, 0, 0, 0, lparm);

    return DAGUE_HOOK_RETURN_ASYNC;
}
