/*
 * Copyright (c) 2010-2015 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 *
 * @precisions normal z -> z c d s
 *
 */

#ifndef _hsa_zgemm_h_
#define _hsa_zgemm_h_

#include "dague_config.h"
#include "dague.h"
#include "dague/execution_unit.h"
#include "dague/class/fifo.h"
#include "data_dist/matrix/matrix.h"


int hsa_zgemm( dague_execution_unit_t* eu_context,
               dague_execution_context_t* this_task,
               PLASMA_enum transA, PLASMA_enum transB,
               int M, int N, int K, 
               dague_complex64_t alpha, int lda,
                                        int ldb,
               dague_complex64_t beta,  int ldc );

#endif /* _hsa_zgemm_h_ */
