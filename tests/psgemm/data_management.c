/*
 * Copyright (c) 2009      The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>


#include "cblas.h"
#include <math.h>
#include <pthread.h>

#include <errno.h>

#include "plasma.h"
#include "../src/common.h"
#include "../src/lapack.h"
#include "../src/context.h"
#include "../src/allocate.h"
#include "data_management.h"
#include "dplasma.h"

#ifdef HAVE_SCHED_SETAFFINITY
#include <unistd.h>
#include <linux/unistd.h>
#include <sys/syscall.h>
#endif  /* HAVE_SCHED_SETAFFINITY */

//#define A(m,n) &((float*)descA.mat)[descA.bsiz*(m)+descA.bsiz*descA.lmt*(n)]
static inline void * plasma_A(PLASMA_desc * Pdesc, int m, int n)
{
    return &((float*)Pdesc->mat)[Pdesc->bsiz*(m)+Pdesc->bsiz*Pdesc->lmt*(n)];

}

static int ddesc_compute_vals( DPLASMA_desc * Ddesc )
{
    int i;
    int nbstile_r;
    int nbstile_c;
    
    /* computing colRANK and rowRANK */
    Ddesc->colRANK = 0;
    Ddesc->rowRANK = 0;
    i = Ddesc->mpi_rank;
    
    /* find rowRANK */
    while ( i >= Ddesc->GRIDcols)
    {
        Ddesc->rowRANK = Ddesc->rowRANK + 1;
        i = i - Ddesc->GRIDcols;
    }
    /* affect colRANK */
    Ddesc->colRANK = i;
        
    /* computing the number of rows of super-tile */
    nbstile_r = Ddesc->lmt / Ddesc->nrst;
    if((Ddesc->lmt % Ddesc->nrst) != 0)
        nbstile_r++;
    
    /* computing the number of colums of super-tile*/
    nbstile_c = Ddesc->lnt / Ddesc->ncst;
    if((Ddesc->lnt % Ddesc->ncst) != 0)
        nbstile_c++;
    
     printf("matrix is super-tiled in (%d, %d); ", nbstile_r, nbstile_c);

    /*  nbt = Ddesc->lmt * Ddesc->lnt;  total number of tiles */ 
    if ( Ddesc->GRIDrows > nbstile_r || Ddesc->GRIDcols > nbstile_c)
    {
        printf("The process grid chosen is %dx%d, supertiling is %d, %d\n", Ddesc->GRIDrows, Ddesc->GRIDcols, nbstile_r, nbstile_c);
        return -1;
    }
    
    /* find the number of tiles this process will handle */
    Ddesc->nb_elem_r = 0;
    i = Ddesc->rowRANK * Ddesc->nrst;
    while ( i < Ddesc->lmt)
    {
        if ( (i  + (Ddesc->nrst)) < Ddesc->lmt)
        {
            Ddesc->nb_elem_r += (Ddesc->nrst);
            i += (Ddesc->GRIDrows) * (Ddesc->nrst);
            continue;
        }
        Ddesc->nb_elem_r += ((Ddesc->lmt) - i);
        break;
    }
    
    Ddesc->nb_elem_c = 0;
    i = Ddesc->colRANK * Ddesc->ncst;
    while ( i < Ddesc->lnt)
    {
        if ( (i  + (Ddesc->ncst)) < Ddesc->lnt)
        {
            Ddesc->nb_elem_c += (Ddesc->ncst);
            i += (Ddesc->GRIDcols) * (Ddesc->ncst);
            continue;
        }
        Ddesc->nb_elem_c += ((Ddesc->lnt) - i);
        break;
    }
    printf("process %d(%d,%d) handles %d x %d tiles\n", Ddesc->mpi_rank, Ddesc->rowRANK, Ddesc->colRANK, Ddesc->nb_elem_r, Ddesc->nb_elem_c);
    return 0;
}

int dplasma_desc_workspace_allocate( DPLASMA_desc * Ddesc ) 
{
    Ddesc->mat = malloc(sizeof(float) * Ddesc->nb_elem_c * Ddesc->nb_elem_r * Ddesc->bsiz);
    return 0;
}

int dplasma_desc_init(const PLASMA_desc * Pdesc, DPLASMA_desc * Ddesc)
{
    Ddesc->dtyp = Pdesc->dtyp;
    Ddesc->mb = Pdesc->mb;
    Ddesc->nb = Pdesc->nb;
    Ddesc->bsiz = Pdesc->bsiz ;
    Ddesc->lm = Pdesc->lm ;
    Ddesc->ln = Pdesc->ln ;
    Ddesc->lmt = Pdesc->lmt ;
    Ddesc->lnt = Pdesc->lnt ;
    Ddesc->i = Pdesc->i ;
    Ddesc->j = Pdesc->j ;
    Ddesc->m = Pdesc->m  ;
    Ddesc->n = Pdesc->n ;
    Ddesc->mt = Pdesc->mt ;
    Ddesc->nt = Pdesc->nt ;
    return ddesc_compute_vals( Ddesc );
}

int dplasma_desc_bcast(const PLASMA_desc * Pdesc, DPLASMA_desc * Ddesc)
{
#ifdef USE_MPI
    int tmp_ints[20];

    if (Ddesc->mpi_rank == 0) /* send data */
    {
        Ddesc->dtyp= tmp_ints[0] = Pdesc->dtyp;
        Ddesc->mb = tmp_ints[1] = Pdesc->mb;
        Ddesc->nb = tmp_ints[2] = Pdesc->nb;
        Ddesc->bsiz  = tmp_ints[3] = Pdesc->bsiz ;
        Ddesc->lm  = tmp_ints[4] = Pdesc->lm ;
        Ddesc->ln  = tmp_ints[5] = Pdesc->ln ;
        Ddesc->lmt  = tmp_ints[6] = Pdesc->lmt ;
        Ddesc->lnt  = tmp_ints[7] = Pdesc->lnt ;
        Ddesc->i = tmp_ints[8] = Pdesc->i ;
        Ddesc->j = tmp_ints[9] = Pdesc->j ;
        Ddesc->m = tmp_ints[10] = Pdesc->m  ;
        Ddesc->n = tmp_ints[11] = Pdesc->n ;
        Ddesc->mt  = tmp_ints[12] = Pdesc->mt ;
        Ddesc->nt  = tmp_ints[13] = Pdesc->nt ;
        tmp_ints[14] = Ddesc->nrst;
        tmp_ints[15] = Ddesc->ncst;            
        tmp_ints[16] = Ddesc->GRIDrows ;
        tmp_ints[17] = Ddesc->GRIDcols ;
        tmp_ints[18] = Ddesc->cores ;
        tmp_ints[19] = Ddesc->nodes ;
        
        MPI_Bcast(tmp_ints, 20, MPI_INT, 0, MPI_COMM_WORLD);
    }
    else /* rank != 0, receive data */
    {
        MPI_Bcast(tmp_ints, 20, MPI_INT, 0, MPI_COMM_WORLD);
        
        Ddesc->dtyp= tmp_ints[0];
        Ddesc->mb = tmp_ints[1];
        Ddesc->nb = tmp_ints[2];
        Ddesc->bsiz  = tmp_ints[3];
        Ddesc->lm  = tmp_ints[4];
        Ddesc->ln  = tmp_ints[5];
        Ddesc->lmt  = tmp_ints[6];
        Ddesc->lnt  = tmp_ints[7];
        Ddesc->i = tmp_ints[8];
        Ddesc->j = tmp_ints[9];
        Ddesc->m = tmp_ints[10];
        Ddesc->n = tmp_ints[11];
        Ddesc->mt  = tmp_ints[12];
        Ddesc->nt  = tmp_ints[13];
        Ddesc->nrst  = tmp_ints[14];
        Ddesc->ncst  = tmp_ints[15];
        Ddesc->GRIDrows  = tmp_ints[16];
        Ddesc->GRIDcols  = tmp_ints[17];
        Ddesc->cores  = tmp_ints[18];
        Ddesc->nodes  = tmp_ints[19];

        if( -1 == ddesc_compute_vals(Ddesc) )
        {
            MPI_Abort(MPI_COMM_WORLD, 2);
        }
    }
    
    return dplasma_desc_workspace_allocate(Ddesc);
#else /* USE_MPI */
    
    fprintf(stderr, "MPI disabled, you should not call this function (%s) in this mode\n", __FUNCTION__);
    return -1;
#endif /* USE_MPI */
}



int generate_matrix(int N, float * A1, float * A2, float * B1, float * B2, int LDA, int NRHS, int LDB)
{
    
    int i, j;
#if USE_LAPACK_RANDOM_MATRIX 
    int IONE=1;
    int info;
    int ISEED[4] = {0,0,0,1};   /* initial seed for dlarnv() */
    int LDBxNRHS = LDB*NRHS;    
    int NminusOne = N-1;
    float* D; float* WORK;
    
    WORK = (float*) malloc(2*LDA*sizeof(float));
    D = (float*) malloc(LDA*sizeof(float));
    
    dlarnv(&IONE, ISEED, &LDA, D);
    dlagsy(&N, &NminusOne, D, A1, &LDA, ISEED, WORK, &info);
    for (i = 0; i < N; i++) A1[LDA*i+i] += 10*N;
    memcpy(A2, A1, LDA*N*sizeof(float));
    dlarnv(&IONE, ISEED, &LDBxNRHS, B1);
    memcpy(B2, B1, LDB*NRHS*sizeof(float));
    free(WORK); free(D);
#else
    for ( i = 0; i < N; i++)
        for ( j = i; j < N; j++) {
            A2[LDA*j+i] = A1[LDA*j+i] = (float)rand() / RAND_MAX;
            A2[LDA*i+j] = A1[LDA*i+j] = A1[LDA*j+i];
        }
    for ( i = 0; i < N; i++) {
        A2[LDA*i+i] = A1[LDA*i+i] += 10*N;
    }
    /* Initialize B1 and B2 */
    for ( i = 0; i < N; i++)
        for ( j = 0; j < NRHS; j++)
            B2[LDB*j+i] = B1[LDB*j+i] = (float)rand() / RAND_MAX;
#endif    
    return 0;
}


int tiling(PLASMA_enum * uplo, int N, float *A, int LDA, int NRHS, PLASMA_desc * descA)
{
    int NB, NT;
    int status;
    float *Abdl;
    plasma_context_t *plasma;

    
    plasma = plasma_context_self();
    if (plasma == NULL) {
        plasma_fatal_error("PLASMA_dpotrf", "PLASMA not initialized");
        return PLASMA_ERR_NOT_INITIALIZED;
    }
    /* Check input arguments */
    if (*uplo != PlasmaUpper && *uplo != PlasmaLower) {
        plasma_error("PLASMA_dpotrf", "illegal value of uplo");
        return -1;
    }
    if (N < 0) {
        plasma_error("PLASMA_dpotrf", "illegal value of N");
        return -2;
    }
    if (LDA < max(1, N)) {
        plasma_error("PLASMA_dpotrf", "illegal value of LDA");
        return -4;
    }
    /* Quick return */
    if (max(N, 0) == 0)
        return PLASMA_SUCCESS;

    /* Tune NB depending on M, N & NRHS; Set NBNBSIZE */
    status = plasma_tune(PLASMA_FUNC_DPOSV, N, N, NRHS);
    if (status != PLASMA_SUCCESS) {
        plasma_error("PLASMA_dpotrf", "plasma_tune() failed");
        return status;
    }

    /* Set NT */
    NB = PLASMA_NB;
    NT = (N%NB==0) ? (N/NB) : (N/NB+1);

    /* Allocate memory for matrices in block layout */
    Abdl = (float *)plasma_shared_alloc(plasma, NT*NT*PLASMA_NBNBSIZE, PlasmaRealFloat);
    if (Abdl == NULL) {
        plasma_error("PLASMA_dpotrf", "plasma_shared_alloc() failed");
        return PLASMA_ERR_OUT_OF_RESOURCES;
    }

    /*PLASMA_desc*/ *descA = plasma_desc_init(
                                             Abdl, PlasmaRealFloat,
                                             PLASMA_NB, PLASMA_NB, PLASMA_NBNBSIZE,
                                             N, N, 0, 0, N, N);

    plasma_parallel_call_3(plasma_lapack_to_tile,
                           float*, A,
                           int, LDA,
                           PLASMA_desc, *descA);

    //printf("matrix tiled in %dx%d\n", descA->lmt, descA->lnt);
    return 0;
    
}

int untiling(PLASMA_enum * uplo, int N, float *A, int LDA, PLASMA_desc * descA)
{
    plasma_context_t *plasma;
        
    plasma = plasma_context_self();
    if (plasma == NULL) {
        plasma_fatal_error("PLASMA_dpotrf", "PLASMA not initialized");
        return PLASMA_ERR_NOT_INITIALIZED;
    }
    /* Check input arguments */
    if (*uplo != PlasmaUpper && *uplo != PlasmaLower) {
        plasma_error("PLASMA_dpotrf", "illegal value of uplo");
        return -1;
    }
    if (N < 0) {
        plasma_error("PLASMA_dpotrf", "illegal value of N");
        return -2;
    }
    if (LDA < max(1, N)) {
        plasma_error("PLASMA_dpotrf", "illegal value of LDA");
        return -4;
    }
    /* Quick return */
    if (max(N, 0) == 0)
        return PLASMA_SUCCESS;
 
    plasma_parallel_call_3(plasma_tile_to_lapack,
                           PLASMA_desc, *descA,
                           float*, A,
                           int, LDA);
    
    //printf("matrix untiled from %dx%d\n", descA->lmt, descA->lnt);
    return PLASMA_SUCCESS;
    
}


int dplasma_get_rank_for_tile(DPLASMA_desc * Ddesc, int m, int n)
{
    int stc, cr;
    int str, rr;
    int res;

    /* for tile (m,n), first find coordinate of process in
       process grid which possess the tile in block cyclic dist */
    str = m / Ddesc->nrst; /* (m,n) is in super-tile (str, stc)*/
    stc = n / Ddesc->ncst;
    
    rr = str % Ddesc->GRIDrows;
    cr = stc % Ddesc->GRIDcols;
    /* P(rr, cr) has the tile, compute the mpi rank*/
    res = rr * Ddesc->GRIDcols + cr;
    /* printf("tile (%d, %d) belongs to process %d [%d,%d] in a grid of %dx%d\n", */
/*            m, n, res, rr, cr, Ddesc->GRIDrows, Ddesc->GRIDcols); */
    return res;
}


/* #if 0 /\*def USE_MPI*\/ */
/* /\* empty stub for now, should allow for async data transfer from recv side *\/ */
/* void * dplasma_get_tile_async(DPLASMA_desc *Ddesc, int m, int n, MPI_Request *req) */
/* { */
    
/*     return NULL; */
/* } */

/* void * dplasma_get_tile(DPLASMA_desc *Ddesc, int m, int n) */
/* { */
/*     int tile_rank; */
    
/*     tile_rank = dplasma_get_rank_for_tile(Ddesc, m, n); */
/*     if(Ddesc->mpi_rank == tile_rank) */
/*     { */
/*         //        printf("%d get_local_tile (%d, %d)\n", Ddesc->mpi_rank, m, n); */
/*         return dplasma_get_local_tileDdesc, m, n); */
/*     } */
/* #ifdef USE_MPI */
/*     printf("%d get_remote_tile (%d, %d) from %d\n", Ddesc->mpi_rank, m, n, tile_rank); */
/*     MPI_Recv(plasma_A((PLASMA_desc *) Ddesc, m, n), Ddesc->bsiz, MPI_DOUBLE, tile_rank, 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE); */
/*     return plasma_A((PLASMA_desc *)Ddesc, m, n); */
/* #else */
/*     fprintf(stderr, "MPI disabled, you should not call this function (%s) in this mode\n", __FUNCTION__); */
/*     return NULL; */
/* #endif */
/* } */

/* #else  */
/* /\*#define dplasma_get_local_tile_s dplasma_get_local_tile*\/ */
/* #endif */

void * dplasma_get_local_tile_s(DPLASMA_desc * Ddesc, int m, int n)
{
    int pos;
    int nb_elem_r, last_c_size;

    assert(Ddesc->mpi_rank == dplasma_get_rank_for_tile(Ddesc, m, n));

    /**********************************/

    nb_elem_r = Ddesc->nb_elem_r * Ddesc->ncst; /* number of tiles per column of super-tile */

    pos = nb_elem_r * ((n / Ddesc->ncst)/ Ddesc->GRIDcols); /* pos is currently at head of supertile (0xA) */

    if (n >= ((Ddesc->lnt/Ddesc->ncst)*Ddesc->ncst )) /* tile is in the last column of super-tile */
        {
            last_c_size = (Ddesc->lnt % Ddesc->ncst) * Ddesc->nrst; /* number of tile per super tile in last column */
        }
    else
        {
            last_c_size = Ddesc->ncst * Ddesc->nrst;
        }
    pos += (last_c_size * ((m / Ddesc->nrst) / Ddesc->GRIDrows ) ); /* pos is at head of supertile (BxA) containing (m,n)  */
    
    /* if tile (m,n) is in the last row of super tile and this super tile is smaller than others */
    if (m >= ((Ddesc->lmt/Ddesc->nrst)*Ddesc->nrst))
        {           
            last_c_size = Ddesc->lmt % Ddesc->nrst;
        }
    else
        {
            last_c_size = Ddesc->nrst;
        }
    pos += ((n % Ddesc->ncst) * last_c_size); /* pos is at (B, n)*/
    pos += (m % Ddesc->nrst); /* pos is at (m,n)*/

    //printf("get tile (%d, %d) is at pos %d\t(ptr %p, base %p)\n", m, n, pos*Ddesc->bsiz,&(((float *) Ddesc->mat)[pos * Ddesc->bsiz]), Ddesc->mat);
    /************************************/
    return &(((float *) Ddesc->mat)[pos * Ddesc->bsiz]);
}

int dplasma_set_local_tile(DPLASMA_desc * Ddesc, int m, int n, void * buff)
{
    float * tile;
    tile = dplasma_get_local_tile(Ddesc, m, n);
    if (tile == NULL)
        {
            return 2;
        }
    memcpy( tile, buff, Ddesc->bsiz*sizeof(float));
    return 0;
}

#ifdef USE_MPI
static int nb_request(DPLASMA_desc * Ddesc, int rank)
{
    int nb_req = 0; //number of request
    int nbr_c;      // number of request per column
    int str;        // number of super tile per column
    int i, j, r;
    int colr, rowr;
    if (rank == 0)
        {
            for( i = 1; i < (Ddesc->GRIDcols * Ddesc->GRIDrows) ; i++)
                {
                    j = nb_request(Ddesc, i);
                    nb_req += j;
                    //                    printf("nb_request adjust for rank 0 to %d (+ %d requests to rank %d)\n", nb_req, j, i);
                }
            return nb_req;
        }
    colr = 0;
    rowr = 0;
    r = rank;
    /* find rowRANK for rank */
    while ( r >= Ddesc->GRIDcols)
        {
            rowr++;
            r = r - Ddesc->GRIDcols;
        }
    /* find colRANK */
    colr = r;

    
    str = Ddesc->lmt / Ddesc->nrst; // number of super tile in a column
    if (Ddesc->lmt % Ddesc->nrst)
        str++;

    str = str - rowr; 
    nbr_c = str / Ddesc->GRIDrows;
    if (str % Ddesc->GRIDrows)
        nbr_c++;

    i = colr * Ddesc->ncst;
    while(i < Ddesc->lnt)
        {
            if (i + Ddesc->ncst > Ddesc->lnt)
                {
                    nb_req = nb_req + ( nbr_c * (Ddesc->lnt - i));
                    return nb_req;
                }
            nb_req = nb_req + (nbr_c * Ddesc->ncst);
            i+=(Ddesc->ncst * Ddesc->GRIDcols); 
        }
    return nb_req;
}
#endif

#ifdef USE_MPI
int distribute_data(PLASMA_desc * Pdesc, DPLASMA_desc * Ddesc, MPI_Request ** reqs, int * req_count)
{
    
    int i, j, k, nb, pos, rank;
    int tile_size, str, stc;
    float * target;
    pos = 0;
    k = 0;
    *req_count = nb_request(Ddesc, Ddesc->mpi_rank);
    /*printf("number of request for proc %d: %d\n", Ddesc->mpi_rank, *req_count);*/
    *reqs = (MPI_Request *)malloc((*req_count) * sizeof(MPI_Request));
    if (NULL == *reqs)
    {
        printf("memory allocation failed\n");
        exit(2);
    }
    if (Ddesc->mpi_rank == 0)
    {
        str = Ddesc->lmt / Ddesc->nrst; // number of super tile in a column
        if (Ddesc->lmt % Ddesc->nrst)
            str++;
        stc = Ddesc->lnt / Ddesc->ncst; // number of super tile in a row
        if (Ddesc->lnt % Ddesc->ncst)
            stc++;
        for (i = 0 ; i < stc; i++) /* for each super tile column */
            for (j = 0 ; j < str ; j++) /* for each super tile row in that column */
            {
                rank = dplasma_get_rank_for_tile(Ddesc, j*Ddesc->nrst, i*Ddesc->ncst);
                //  printf("tile (%d,%d) belongs to %d\n", j*Ddesc->nrst, i*Ddesc->ncst, rank);
                if (rank == 0) /* this tile belongs to me */
                {
                    tile_size = min(Ddesc->nrst, Ddesc->lmt-(j*Ddesc->nrst));
                    //printf("number of tile to copy at once: %d, ", tile_size);
                    target = (float *) plasma_A(Pdesc, j*Ddesc->nrst, i*Ddesc->ncst);
                    //printf(" -->tile (%d, %d) for self, memcpy at pos %d\n", j*Ddesc->nrst , i*Ddesc->ncst, pos );
                    for (nb = 0 ; nb < min(Ddesc->ncst, Ddesc->lnt - (i*Ddesc->ncst)) ; nb++)
                    {
                        //printf("start nb=%d, end %d, ", nb , min(Ddesc->ncst, Ddesc->lnt - (i*Ddesc->ncst)));
                        //printf("target at %e\n", target[0]);
                        memcpy(&(((float *)Ddesc->mat)[pos]), target , tile_size * Ddesc->bsiz*(sizeof(float)));
                        //printf("--->memcpy %d eme tile to %d (%ld bytes)\n", nb +1, pos, tile_size * Ddesc->bsiz*(sizeof(float)));
                        pos += (Ddesc->bsiz * tile_size);
                        target += Ddesc->lmt * Ddesc->bsiz;
                    }
                    
                    continue;
                }
                tile_size = min(Ddesc->nrst, Ddesc->lmt-(j*Ddesc->nrst));
                target = (float *)plasma_A(Pdesc, j*Ddesc->nrst, i*Ddesc->ncst);
                for (nb = 0 ; nb < min(Ddesc->ncst, Ddesc->lnt - (i*Ddesc->ncst)) ; nb++)
                {                                        
                    MPI_Isend(target, tile_size * Ddesc->bsiz, MPI_DOUBLE, rank, 1, MPI_COMM_WORLD, &((*reqs)[k]));
                    k++;
                    target += Ddesc->lmt * Ddesc->bsiz;
                    if(0 == (k % 4)) 
                    {
                        MPI_Waitall(k, *reqs, MPI_STATUSES_IGNORE);
                        k = 0;
                    }
                }
		if(k)
		{
		    MPI_Waitall(k, *reqs, MPI_STATUSES_IGNORE);
		    k = 0;
		}
            }
    }
    else /* mpi_rank != 0*/
    {
        pos = 0;
        k = 0;
        str = Ddesc->lmt / Ddesc->nrst; // number of super tile in a column
        if (Ddesc->lmt % Ddesc->nrst)
            str++;
        stc = Ddesc->lnt / Ddesc->ncst; // number of super tile in a row
        if (Ddesc->lnt % Ddesc->ncst)
            stc++;
        for (i = 0 ; i < stc; i++) /* for each super tile column */
            for (j = 0 ; j < str ; j++) /* for each super tile row in that column */
            {
                rank = dplasma_get_rank_for_tile(Ddesc, j*Ddesc->nrst, i*Ddesc->ncst);
                if (rank == Ddesc->mpi_rank) /* this tile belongs to me */
                {
                    tile_size = min(Ddesc->nrst, Ddesc->lmt-(j*Ddesc->nrst));
                    
                    for (nb = 0 ; nb < min(Ddesc->ncst, Ddesc->lnt - (i*Ddesc->ncst)) ; nb++)
                    {                                        
                        MPI_Irecv(&(((float*)Ddesc->mat)[pos]), tile_size * Ddesc->bsiz, MPI_DOUBLE, 0, 1, MPI_COMM_WORLD, &((*reqs)[k]));
                        pos += tile_size * Ddesc->bsiz;
                        k++;
			if(0 == (k %4))
			{
				MPI_Waitall(k, *reqs, MPI_STATUSES_IGNORE);
				k = 0;
			}
                    }
		    if(k)
		    {
			    MPI_Waitall(k, *reqs, MPI_STATUSES_IGNORE);
			    k = 0;
		    }
                }
            }
    }
    *req_count = 0;
    free(*reqs);
    *reqs = NULL;
    return 0;
    
}
#endif    

int gather_data(PLASMA_desc * Pdesc, DPLASMA_desc * Ddesc) 
{
#ifdef USE_MPI
    int i, j,  rank;
    int req_count;
    MPI_Request * reqs;

    /* we overallocate for rank>0, but who cares ? */ 
    reqs = malloc(sizeof(MPI_Request) * Ddesc->lmt * Ddesc->lnt);
    req_count = 0;
    
    if ( Ddesc->mpi_rank == 0)
    {
        for (i = 0 ; i < Ddesc->lmt ; i++ )
            for(j = 0; j < Ddesc->lnt ; j++)
            {
                rank = dplasma_get_rank_for_tile(Ddesc, i, j);
                if (rank == 0)
                    memcpy(plasma_A(Pdesc, i, j ), dplasma_get_local_tile(Ddesc, i, j), Ddesc->bsiz * sizeof(float)) ;
                else
                    MPI_Irecv( plasma_A(Pdesc, i, j), Ddesc->bsiz, MPI_DOUBLE, rank, 1, MPI_COMM_WORLD, &reqs[req_count++] );
            }
    }
    else
    {
        for (i = 0 ; i < Ddesc->lmt ; i++ )
            for(j = 0; j < Ddesc->lnt ; j++)
            {
                rank = dplasma_get_rank_for_tile(Ddesc, i, j);
                if (rank == Ddesc->mpi_rank)
                    MPI_Isend( dplasma_get_local_tile(Ddesc, i, j), Ddesc->bsiz, MPI_DOUBLE, 0, 1, MPI_COMM_WORLD, &reqs[req_count++] );
            }
        
    }
    MPI_Waitall(req_count, reqs, MPI_STATUSES_IGNORE);
    free(reqs);
    return 0;
#else
    fprintf(stderr, "MPI disabled, you should not call this function (%s) in this mode\n", __FUNCTION__);
    return -1;
#endif    
}

#ifdef HEAVY_DEBUG
static void print_block(char * stri, int m, int n, float * block, int blength, int total_size)
{
    int i;
    printf("%s (%d,%d)\n", stri, m, n);
    for (i = 0 ; i < min(10, blength) ; i++ )
        printf("%e ", block[i]);
    printf("\n");
    i = total_size - blength;
    for ( ; i < min((total_size - blength) + 10, total_size) ; i++ )
        printf("%e ", block[i]);
    printf("\n\n");
}
#endif

void data_dist_verif(PLASMA_desc * Pdesc, DPLASMA_desc * Ddesc)
{
#ifdef USE_MPI
    int m, n, k, rank;
    float * buff;
    float * buff2;

    for (m = 0 ; m < Ddesc->lmt ; m++)
        for ( n = 0 ; n < Ddesc->lnt ; n++)
        {
            MPI_Barrier(MPI_COMM_WORLD);
            
            rank = dplasma_get_rank_for_tile(Ddesc, m, n);
            
            if ((rank == 0) && (Ddesc->mpi_rank == 0))/* this proc is rank 0 and handles the tile*/
            {
                buff = plasma_A(Pdesc, m, n);
                buff2 = dplasma_get_local_tile(Ddesc, m, n);
                for ( k = 0; k < Ddesc->bsiz ; k++)
                {
                    if(buff[k] != buff2[k])
                    {
                        printf("WARNING: tile(%d, %d) differ !\n", m, n);
                        break;
                    }
                }
                continue;
            }
#ifdef HEAVY_DEBUG
            if(Ddesc->mpi_rank == 0)
                print_block("orig tile ", m, n, plasma_A(Pdesc, m, n), Pdesc->nb, Pdesc->bsiz);
            
            
            if (Ddesc->mpi_rank == rank)
            {
                buff = dplasma_get_local_tile(Ddesc, m, n);
                printf("Check: rank %d has tile %d, %d\n", Ddesc->mpi_rank, m, n);
                print_block("Dist tile", m, n, buff, Ddesc->nb, Ddesc->bsiz);
            }
#endif
        }
    MPI_Barrier(MPI_COMM_WORLD);
    printf("Data verification ended\n");
#else
    fprintf(stderr, "MPI disabled, you should not call this function (%s) in this mode\n", __FUNCTION__);
#endif    
}

int data_dump(DPLASMA_desc * Ddesc){
    FILE * tmpf;
    int i, j, k;
    float * buf;
    tmpf = fopen("tmp_local_data_dump.txt", "w");
    if(NULL == tmpf)
        {
            perror("opening file: tmp_local_data_dump.txt" );
            return -1;
        }
    for (i = 0 ; i < Ddesc->lmt ; i++)
        for ( j = 0 ; j< Ddesc->lnt ; j++)
            {
                if (dplasma_get_rank_for_tile(Ddesc, i, j) == Ddesc->mpi_rank)
                    {
                        buf = (float*)dplasma_get_local_tile(Ddesc, i, j);
                        for (k = 0 ; k < Ddesc->bsiz ; k++)
                            {
                                fprintf(tmpf, "%e ", buf[k]);
                            }
                        fprintf(tmpf, "\n");
                    }
            }
    fclose(tmpf);
    return 0;
}

int plasma_dump(PLASMA_desc * Pdesc){
    FILE * tmpf;
    int i, j, k;
    float * buf;
    tmpf = fopen("tmp_plasma_data_dump.txt", "w");
    if(NULL == tmpf)
        {
            perror("opening file: tmp_plasma_data_dump.txt" );
            return -1;
        }
    for (i = 0 ; i < Pdesc->lmt ; i++)
        for ( j = 0 ; j< Pdesc->lnt ; j++)
            {
                buf = (float*)plasma_A(Pdesc, i, j);
                for (k = 0 ; k < Pdesc->bsiz ; k++)
                    {
                        fprintf(tmpf, "%e ", buf[k]);
                    }
                fprintf(tmpf, "\n");
            }
    fclose(tmpf);
    return 0;
}

#ifdef USE_MPI
int is_data_distributed(DPLASMA_desc * Ddesc, MPI_Request * reqs, int req_count)
{
    if(req_count)
    {
	MPI_Waitall(req_count, reqs, MPI_STATUSES_IGNORE);
    }
    return 1;
}
#endif    

int compare_distributed_tiles(DPLASMA_desc * A, DPLASMA_desc * B, int row, int col, float precision)
{
    int i;
    float * a;
    float * b;
    float c;

    /* check memory locality of the tiles */
    if (   (A->mpi_rank != dplasma_get_rank_for_tile(A, row, col))
        || (B->mpi_rank != dplasma_get_rank_for_tile(B, row, col))
        || (A->mpi_rank != B->mpi_rank))
        {
            printf("Compare tile failed: (%d, %d) is not local to process %d\n", row, col, A->mpi_rank);
            return 0;
        }
    
    /* assign values */
    a = (float *)dplasma_get_local_tile_s(A, row, col);
    b = (float *)dplasma_get_local_tile_s(B, row, col);
    /* compare each value*/
    for(i = 0 ; i < A->bsiz ; i++)
        {
            c = a[i] - b[i];
            if(0.0-precision < c && c < precision)
                continue;
            else
                {
                    printf("difference discovered in matrix. Tile: (%d, %d), position: %d, difference: %f\n", row, col, i, c);
                    return 0;
                }
        }
    return 1;
}

int compare_matrices(DPLASMA_desc * A, DPLASMA_desc * B, float precision)
{
    int i, j, mt, nt, rank;
    int res = 1;
    mt = (A->mt < B->mt) ? A->mt : B->mt;
    nt = (A->nt < B->nt) ? A->nt : B->nt;
    for (i = 0 ; i < mt ; i++)
        for( j = 0 ; i < nt ; j++)
            {
                rank = dplasma_get_rank_for_tile(A, i, j);
                if (rank == A->mpi_rank)
                    {
                        res &= compare_distributed_tiles(A, B, i, j, precision);
                    }
                if (res == 0)
                    return 0;
            }
    return res;
}

int compare_plasma_matrices(PLASMA_desc * A, PLASMA_desc * B, float precision)
{
    int i, j, k, mt, nt;
    float * a;
    float * b;
    float c;

    mt = (A->mt < B->mt) ? A->mt : B->mt;
    nt = (A->nt < B->nt) ? A->nt : B->nt;
    printf("compare matrices of size %d x %d\n", mt, nt);
    for (i = 0 ; i < mt ; i++)
        for( j = 0 ; j < nt ; j++)
            {
                a = (float *)plasma_A(A, i, j);
                b = (float *)plasma_A(B, i, j);
                /* compare each value*/
                for(k = 0 ; k < A->bsiz ; k++)
                    {
                        c = a[k] - b[k];
                        if(0.0-precision < c && c < precision)
                            continue;
                        else
                            {
                                printf("difference discovered in matrix. Tile: (%d, %d), position: %d, difference: %f\n", i, j, k, c);
                                return 0;
                            }
                    }
                printf("tile (%d, %d) passed  (bsiz == %d)\n", i, j, A->bsiz);
            }
    printf("Matrices almost identical\n");
    return 1;
}

/*
 Rnd64seed is a global variable but it doesn't spoil thread safety. All matrix
 generating threads only read Rnd64seed. It is safe to set Rnd64seed before
 and after any calls to create_tile(). The only problem can be caused if
 Rnd64seed is changed during the matrix generation time.
 */
unsigned long long int Rnd64seed = 100;
#define Rnd64_A 6364136223846793005ULL
#define Rnd64_C 1ULL

unsigned long long int
Rnd64_jump(unsigned long long int n) {
  unsigned long long int a_k, c_k, ran;
  int i;

  a_k = Rnd64_A;
  c_k = Rnd64_C;

  ran = Rnd64seed;
  for (i = 0; n; n >>= 1, ++i) {
    if (n & 1)
      ran = a_k * ran + c_k;
    c_k *= (a_k + 1);
    a_k *= a_k;
  }

  return ran;
}

/************************************************************
 *distributed matrix generation
 ************************************************************/
/* affect one tile with random values  */
static void create_tile(DPLASMA_desc * Ddesc, float * position,  int row, int col)
{
    int i, j, first_row, first_col, nb = Ddesc->nb, mn_max = Ddesc->n > Ddesc->m ? Ddesc->n : Ddesc->m;
    float *x = position;
    unsigned long long int ran;

    /* These are global values of first row and column of the tile counting from 0 */
    first_row = row * nb;
    first_col = col * nb;

    for (j = 0; j < nb; ++j) {
      ran = Rnd64_jump( first_row + (first_col + j) * (unsigned long long int)Ddesc->m );

      for (i = 0; i < nb; ++i) {
        x[0] = 0.5 - ran * 5.4210108624275222e-20;
        ran = Rnd64_A * ran + Rnd64_C;
        x += 1;
      }
    }
    /* This is only required for Cholesky: diagonal is bumped by max(M, N) */
    if (row == col) {
      for (i = 0; i < nb; ++i)
        position[i + i * nb] += mn_max;
    }
}

typedef struct tile_coordinate{
    int row;
    int col;
} tile_coordinate_t;

typedef struct dist_tiles{
    DPLASMA_desc * Ddesc;
    int th_id;
    int nb_elements;
    float * starting_position;
} dist_tiles_t;


/*
  given a position in the matrix array, retrieve to which tile it belongs

 */
static void pos_to_coordinate(DPLASMA_desc * Ddesc, float * position, tile_coordinate_t * tile) 
{ 
    int nb_tiles;
    int shift;
    int erow;
    int ecol;
    int rst;
    int cst;
    erow = Ddesc->rowRANK * Ddesc->nrst; /* row of the first tile in memory */ 
    ecol = Ddesc->colRANK * Ddesc->ncst; /* col of the first tile in memory */
    nb_tiles = position - ((float*)Ddesc->mat); /* how many element (float) in the array before the looked up one */
    nb_tiles = nb_tiles / Ddesc->bsiz; /* how many tiles before this position */
    shift = Ddesc->ncst * Ddesc->nb_elem_r; /* nb tiles per colum of super-tile */
    ecol += (nb_tiles / shift) * (Ddesc->GRIDcols * Ddesc->ncst);
    nb_tiles = nb_tiles % shift;

    cst = ((Ddesc->lnt - ecol) < Ddesc->ncst) ? (Ddesc->lnt - ecol) : Ddesc->ncst; /* last super-tile column of the matrix ? */
    shift = cst * Ddesc->nrst; /* super-tile size for the colum where resides the element */

    erow += (nb_tiles / shift) * (Ddesc->GRIDrows * Ddesc->nrst); /* elements in the super-tile starting by tile (erow,ecol) */
    nb_tiles = nb_tiles % shift;
    rst = ((Ddesc->lmt - erow) < Ddesc->nrst) ? (Ddesc->lmt - erow) : Ddesc->nrst; /* last super-tile of the matrix ? */
    ecol += nb_tiles / rst;
    erow += nb_tiles % rst;
    tile->row = erow;
    tile->col = ecol;
}

#ifdef HAVE_SCHED_SETAFFINITY
#define gettid() syscall(__NR_gettid)
#endif

/* thread function for affecting multiple tiles with random values
 * @param : tiles : of type dist_tiles_t

 */
static void * rand_dist_tiles(void * tiles)
{
    int i;
    float * pos;
    tile_coordinate_t current_tile;
    /* bind thread to cpu */
#if defined(HAVE_HWLOC) || defined(HAVE_SCHED_SETAFFINITY)
    int bind_to_proc = ((dist_tiles_t *)tiles)->th_id;
#endif  /* defined(HAVE_HWLOC) || defined(HAVE_SCHED_SETAFFINITY) */
#ifdef HAVE_SCHED_SETAFFINITY
    {
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(bind_to_proc, &cpuset);

        if( -1 == sched_setaffinity(gettid(), sizeof(cpu_set_t), &cpuset) ) {
            printf( "Unable to set the thread affinity (%s)\n", strerror(errno) );
        }
    }
#endif  /* HAVE_SCHED_SETAFFINITY */
    printf("generating matrix on process %d, thread %d: %d tiles\n",
           ((dist_tiles_t*)tiles)->Ddesc->mpi_rank,
           ((dist_tiles_t*)tiles)->th_id,
           ((dist_tiles_t*)tiles)->nb_elements);
    
    pos = ((dist_tiles_t*)tiles)->starting_position;
    for (i = 0 ; i < ((dist_tiles_t*)tiles)->nb_elements ; i++)
        {
            pos_to_coordinate(((dist_tiles_t*)tiles)->Ddesc, pos, &current_tile);
            create_tile(((dist_tiles_t*)tiles)->Ddesc, pos, current_tile.row, current_tile.col);
            pos += ((dist_tiles_t*)tiles)->Ddesc->bsiz;
        }
    return NULL;
}

/* affecting the complete local view of a distributed matrix with random values */
int rand_dist_matrix(DPLASMA_desc * Ddesc)
{
    dist_tiles_t * tiles;
    int i;
    float * pos = Ddesc->mat;
    pthread_t *threads;
    pthread_attr_t thread_attr;
    Ddesc->lm = Ddesc->lmt * Ddesc->mb;
    Ddesc->ln = Ddesc->lnt * Ddesc->nb;
    Ddesc->m = Ddesc->lm;
    Ddesc->n = Ddesc->ln;
    printf("generated matrix size: %d x %d\n", Ddesc->lm, Ddesc->ln);
    
    if (Ddesc->cores > 1)
        {
            pthread_attr_init(&thread_attr);
            pthread_attr_setscope(&thread_attr, PTHREAD_SCOPE_SYSTEM);
#ifdef __linux
            pthread_setconcurrency(Ddesc->cores);
#endif  /* __linux */
        }
    tiles = malloc(sizeof(dist_tiles_t) * Ddesc->cores);
    for ( i = 0 ; i < Ddesc->cores ; i++ )
        {
            tiles[i].th_id = i;
            tiles[i].Ddesc = Ddesc;
            tiles[i].nb_elements = (Ddesc->nb_elem_r * Ddesc->nb_elem_c) / Ddesc->cores;
            tiles[i].starting_position = pos;
            pos += tiles[i].nb_elements * Ddesc->bsiz;
        }
    tiles[i -1].nb_elements+= (Ddesc->nb_elem_r * Ddesc->nb_elem_c) % Ddesc->cores;

    if (Ddesc->cores > 1)
        {
            threads = malloc(Ddesc->cores * sizeof(pthread_t));
            for ( i = 1 ; i < Ddesc->cores ; i++)
                {
                    pthread_create( &(threads[i-1]),
                                    &thread_attr,
                                    (void* (*)(void*))rand_dist_tiles,
                                    (void*)&(tiles[i]));
                }
        }

    rand_dist_tiles((void*) &(tiles[0]));

    if (Ddesc->cores > 1)
        {
            for(i = 0 ; i < Ddesc->cores - 1 ; i++)
                pthread_join(threads[i],NULL);
            free (threads);
        }
    free (tiles);
    return 0;
}


int dplasma_description_init( DPLASMA_desc * Ddesc, int LDA, int LDB, int NRHS, PLASMA_enum uplo)
{

    int i,  status;
    plasma_context_t *plasma;
    int nbstile_r;
    int nbstile_c;

    plasma = plasma_context_self();
    if (plasma == NULL) {
        plasma_fatal_error("PLASMA_dpotrf", "PLASMA not initialized");
        return PLASMA_ERR_NOT_INITIALIZED;
    }
    /* Check input arguments */
    if (uplo != PlasmaUpper && uplo != PlasmaLower) {
        plasma_error("PLASMA tune", "illegal value of uplo");
        return -1;
    }
    if (Ddesc->n < 0) {
        plasma_error("PLASMA tune", "illegal value of N");
        return -2;
    }
    if (LDA < max(1, Ddesc->n)) {
        plasma_error("PLASMA tune", "illegal value of LDA");
        return -4;
    }
    /* Quick return */
    if (max(Ddesc->n, 0) == 0)
        {
            printf("empty matrix (size 0)\n");
            return PLASMA_SUCCESS;
        }

    /* Tune NB depending on M, N & NRHS; Set NBNBSIZE */
    status = plasma_tune(PLASMA_FUNC_DPOSV, Ddesc->n, Ddesc->n, NRHS);
    if (status != PLASMA_SUCCESS) {
        plasma_error("PLASMA_dpotrf", "plasma_tune() failed");
        return status;
    }

    /* Set NB, NT, BSIZ */
    Ddesc->nb = PLASMA_NB;
    Ddesc->nt = ((Ddesc->n)%(Ddesc->nb)==0) ? ((Ddesc->n)/(Ddesc->nb)) : ((Ddesc->n)/(Ddesc->nb) + 1);
    Ddesc->bsiz = PLASMA_NBNBSIZE;
    // Matrix properties
    Ddesc->dtyp = PlasmaRealFloat;
    Ddesc->mb = Ddesc->nb;
    // Large matrix parameters
    Ddesc->lm = Ddesc->n;
    Ddesc->ln = Ddesc->n;
    // Large matrix derived parameters
    Ddesc->lmt = ((Ddesc->lm)%(Ddesc->nb)==0) ? ((Ddesc->lm)/(Ddesc->nb)) : ((Ddesc->lm)/(Ddesc->nb) + 1);
    Ddesc->lnt = ((Ddesc->ln)%(Ddesc->nb)==0) ? ((Ddesc->ln)/(Ddesc->nb)) : ((Ddesc->ln)/(Ddesc->nb) + 1);
    // Submatrix parameters
    Ddesc->i = 0;
    Ddesc->j = 0;
    Ddesc->m = Ddesc->n;
    Ddesc->n = Ddesc->n;
    // Submatrix derived parameters
    Ddesc->mt = ((Ddesc->i)+(Ddesc->m)-1)/(Ddesc->nb) - (Ddesc->i)/(Ddesc->nb) + 1;
    Ddesc->nt = ((Ddesc->j)+(Ddesc->n)-1)/(Ddesc->nb) - (Ddesc->j)/(Ddesc->nb) + 1;


    /* computing colRANK and rowRANK */
    Ddesc->rowRANK = (Ddesc->mpi_rank)/(Ddesc->GRIDcols);
    Ddesc->colRANK = (Ddesc->mpi_rank)%(Ddesc->GRIDcols);


    /* computing the number of rows of super-tile */
    nbstile_r = Ddesc->lmt / Ddesc->nrst;
    if((Ddesc->lmt % Ddesc->nrst) != 0)
        nbstile_r++;

    /* computing the number of colums of super-tile*/
    nbstile_c = Ddesc->lnt / Ddesc->ncst;
    if((Ddesc->lnt % Ddesc->ncst) != 0)
        nbstile_c++;

    if ( Ddesc->GRIDrows > nbstile_r || Ddesc->GRIDcols > nbstile_c)
        {
            printf("The process grid chosen is %dx%d, block distribution choosen is %d, %d : cannot generate matrix \n",
                   Ddesc->GRIDrows, Ddesc->GRIDcols, nbstile_r, nbstile_c);
            return -1;
        }
    printf("matrix to be generated distributed by block of %d x %d tiles \n", nbstile_r, nbstile_c);    

    /* find the number of tiles this process will handle */
    Ddesc->nb_elem_r = 0;
    i = Ddesc->rowRANK * Ddesc->nrst; /* row coordinate of the first tile to handle */
    while ( i < Ddesc->lmt)
        {
            if ( (i  + (Ddesc->nrst)) < Ddesc->lmt)
                {
                    Ddesc->nb_elem_r += (Ddesc->nrst);
                    i += ((Ddesc->GRIDrows) * (Ddesc->nrst));
                    continue;
                }
            Ddesc->nb_elem_r += ((Ddesc->lmt) - i);
            break;
        }

    Ddesc->nb_elem_c = 0;
    i = Ddesc->colRANK * Ddesc->ncst;
    while ( i < Ddesc->lnt)
        {
            if ( (i  + (Ddesc->ncst)) < Ddesc->lnt)
                {
                    Ddesc->nb_elem_c += (Ddesc->ncst);
                    i += (Ddesc->GRIDcols) * (Ddesc->ncst);
                    continue;
                }
            Ddesc->nb_elem_c += ((Ddesc->lnt) - i);
            break;
        }
    printf("process %d(%d,%d) handles %d x %d tiles\n",
           Ddesc->mpi_rank, Ddesc->rowRANK, Ddesc->colRANK, Ddesc->nb_elem_r, Ddesc->nb_elem_c);

    /* Allocate memory for matrices in block layout */
    Ddesc->mat =(float *)plasma_shared_alloc(plasma, Ddesc->nb_elem_r*Ddesc->nb_elem_c * Ddesc->bsiz, PlasmaRealFloat);
    if (Ddesc->mat == NULL)
        {
            plasma_error("PLASMA_dpotrf", "plasma_shared_alloc() failed");
            return PLASMA_ERR_OUT_OF_RESOURCES;
        }
    return 0;

}
