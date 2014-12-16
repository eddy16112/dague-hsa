/*
 * Copyright (c) 2011-2014 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 */

#include "dague_config.h"
#include "dague_internal.h"
#include "data_dist/matrix/matrix.h"
#include "reduce_col.h"
#include "reduce_row.h"

dague_handle_t*
dague_reduce_col_New( const tiled_matrix_desc_t* src,
                      tiled_matrix_desc_t* dest,
                      dague_operator_t operator,
                      void* op_data )
{
    dague_handle_t* dague;

    dague = (dague_handle_t*)dague_reduce_col_new( src, dest, operator, op_data, 0, 0, src->lnt, src->lmt );
    return dague;
}

void dague_reduce_col_Destruct( dague_handle_t *o )
{
    DAGUE_INTERNAL_HANDLE_DESTRUCT(o);
}

dague_handle_t*
dague_reduce_row_New( const tiled_matrix_desc_t* src,
                      tiled_matrix_desc_t* dest,
                      dague_operator_t operator,
                      void* op_data )
{
    dague_handle_t* dague;

    dague = (dague_handle_t*)dague_reduce_row_new( src, dest, operator, op_data );
    return dague;
}

void dague_reduce_row_Destruct( dague_handle_t *o )
{
    DAGUE_INTERNAL_HANDLE_DESTRUCT(o);
}

