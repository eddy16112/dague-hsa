/*
 * Copyright (c) 2009-2015 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 */

#if !defined(_MISSING_BT_REDUCTION_WRAPPER_H_)
#define _MISSING_BT_REDUCTION_WRAPPER_H_

#include "dague.h"
#include "dague/data_distribution.h"
#include "data_dist/matrix/matrix.h"

dague_handle_t *BT_reduction_new(tiled_matrix_desc_t *A, int size, int nt);

#endif
