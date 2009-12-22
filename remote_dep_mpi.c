/*
 * Copyright (c) 2009      The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 */

/* /!\  THIS FILE IS NOT INTENDED TO BE COMPILED ON ITS OWN
 *      It should be included from remote_dep.c if USE_MPI is defined
 */

#include <mpi.h>


/* >>>>TODO: smart use of eu_context instead of ugly globals <<<<<< */
static MPI_Comm dep_comm;
static MPI_Request dep_req;
#define dep_dtt MPI_BYTE
#define dep_count sizeof(dplasma_execution_context_t)
static dplasma_execution_context_t dep_buff;

int dplasma_dependency_management_init(dplasma_execution_unit_t* eu_context)
{
    MPI_Comm_dup(MPI_COMM_WORLD, &dep_comm);
    MPI_Recv_init(&dep_buff, dep_count, dep_dtt, MPI_ANY_SOURCE, REMOTE_DEP_ACTIVATE_TAG, dep_comm, &dep_req);
    MPI_Start(&dep_req);
    return 0;
}

int dplasma_dependency_management_fini(dplasma_execution_unit_t* eu_context)
{
    MPI_Cancel(&dep_req);
    MPI_Request_free(&dep_req);
    MPI_Comm_free(&dep_comm);
    return 0;
}



int dplasma_remote_dep_activate(dplasma_execution_unit_t* eu_context,
                                const dplasma_execution_context_t* origin,
                                const param_t* origin_param,
                                dplasma_execution_context_t* exec_context,
                                const param_t* dest_param )
{
    int rank; 
    
    rank = remote_dep_compute_grid_rank(eu_context, origin, exec_context);
    assert(rank >= 0);
    return MPI_Send((void*) origin, dep_count, dep_dtt, rank, REMOTE_DEP_ACTIVATE_TAG, dep_comm);
}


int dplasma_remote_dep_progress(dplasma_execution_unit_t* eu_context)
{
    MPI_Status status;
    int flag;
    
    MPI_Test(&dep_req, &flag, &status);
    if(flag)
    {
        
        
    }
    else
    {
        
    }
    return 0;
}


