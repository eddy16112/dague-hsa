#include <pthread.h>
#include <stdarg.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "lifo.h"
#include "os-spec-timing.h"
#include "bindthread.h"

static unsigned int NBELT = 8192;
static unsigned int NBTIMES = 1000000;

static void fatal(const char *format, ...)
{
    va_list va;
    va_start(va, format);
    vprintf(format, va);
    va_end(va);
    raise(SIGABRT);
}

static dague_atomic_lifo_t lifo1;
static dague_atomic_lifo_t lifo2;

typedef struct {
    dague_list_item_t list;
    unsigned int base;
    unsigned int nbelt;
    unsigned int elts[1];
} elt_t;

static elt_t *create_elem(int base)
{
    elt_t *elt;
    size_t r;
    unsigned int j;

    r = rand() % 1024;
    elt = (elt_t*)malloc( r * sizeof(unsigned int) + sizeof(elt_t) );
    elt->base = base;
    elt->nbelt = r;
    for(j = 0; j < r; j++)
        elt->elts[j] = elt->base + j;
    DAGUE_LIST_ITEM_SINGLETON( (dague_list_item_t *)elt );
    
    return elt;
}

static void check_elt(elt_t *elt)
{
    unsigned int j;
    for(j = 0; j < elt->nbelt; j++)
        if( elt->elts[j] != elt->base + j ) 
            fatal(" ! Error: element number %u of elt with base %u is corrupt\n", j, elt->base);
}

static void check_translate_outoforder(dague_atomic_lifo_t *l1,
                                    dague_atomic_lifo_t *l2,
                                    const char *lifo1name,
                                    const char *lifo2name)
{
    static unsigned char *seen = NULL;
    unsigned int e;
    elt_t *elt;

    printf(" - pop them from %s, check they are ok, push them back in %s, and check they are all there\n",
           lifo1name, lifo2name);

    if( NULL == seen ) 
        seen = (unsigned char *)calloc(1, NBELT);
    else
        memset(seen, 0, NBELT);

    for(e = 0; e < NBELT; e++) {
        elt = (elt_t *)dague_atomic_lifo_pop( l1 );
        if( NULL == elt ) 
            fatal(" ! Error: there are only %u elements in %s -- expecting %u\n", e+1, lifo1name, NBELT);
        DAGUE_LIST_ITEM_SINGLETON( elt );
        check_elt( elt );
        dague_atomic_lifo_push( l2, (dague_list_item_t *)elt );
        if( elt->base >= NBELT )
            fatal(" ! Error: base of the element %u of %s is outside boundaries\n", e, lifo1name);
        if( seen[elt->base] == 1 ) 
            fatal(" ! Error: the element %u appears at least twice in %s\n", elt->base, lifo1name);
        seen[elt->base] = 1;
    }
    /* No need to check that seen[e] == 1 for all e: this is captured by if (NULL == elt) */
    if( (elt = (elt_t*)dague_atomic_lifo_pop( l1 )) != NULL ) 
        fatal(" ! Error: unexpected element of base %u in %s: it should be empty\n", 
              elt->base, lifo1name);
}

static void check_translate_inorder(dague_atomic_lifo_t *l1,
                                    dague_atomic_lifo_t *l2,
                                    const char *lifo1name,
                                    const char *lifo2name)
{
    unsigned int e;
    elt_t *elt;
    printf(" - pop them from %s, check they are ok, and push them back in %s\n",
           lifo1name, lifo2name);

    elt = (elt_t *)dague_atomic_lifo_pop( l1 );
    if( NULL == elt ) 
        fatal(" ! Error: expecting a full list in %s, got an empty one...\n", lifo1name);
    DAGUE_LIST_ITEM_SINGLETON( elt );
    if( elt->base == 0 ) {
        check_elt( elt );
        dague_atomic_lifo_push( l2, (dague_list_item_t *)elt );
        for(e = 1; e < NBELT; e++) {
            elt = (elt_t *)dague_atomic_lifo_pop( l1 );
            if( NULL == elt ) 
                fatal(" ! Error: element number %u was not found at its position in %s\n", e, lifo1name);
            DAGUE_LIST_ITEM_SINGLETON( elt );
            if( elt->base != e )
                fatal(" ! Error: element number %u has its base corrupt\n", e);
            check_elt( elt );
            dague_atomic_lifo_push( l2, (dague_list_item_t *)elt );
        }
    } else if( elt->base == NBELT-1 ) {
        check_elt( elt );
        dague_atomic_lifo_push( l2, (dague_list_item_t *)elt );
        for(e = NBELT-2; ; e--) {
            elt = (elt_t *)dague_atomic_lifo_pop( l1 );
            if( NULL == elt ) 
                fatal(" ! Error: element number %u was not found at its position in %s\n", e, lifo1name);
            DAGUE_LIST_ITEM_SINGLETON( elt );
            if( elt->base != e )
                fatal(" ! Error: element number %u has its base corrupt\n", e);
            check_elt( elt );
            dague_atomic_lifo_push( l2, (dague_list_item_t *)elt );
            if( 0 == e )
                break;
        }
    } else {
        fatal(" ! Error: the lifo %s does not start with 0 or %u\n", lifo1name, NBELT-1);
    }
}



static pthread_mutex_t heavy_synchro_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  heavy_synchro_cond = PTHREAD_COND_INITIALIZER;
static unsigned int    heavy_synchro = 0;

static void *translate_elements_random(void *params)
{
    unsigned int i;
    dague_list_item_t *e;
    uint64_t *p = (uint64_t*)params;
    dague_time_t start, end;

    dague_bindthread( (int)*p );

    pthread_mutex_lock(&heavy_synchro_lock);
    while( heavy_synchro == 0 ) {
        pthread_cond_wait(&heavy_synchro_cond, &heavy_synchro_lock);
    }
    pthread_mutex_unlock(&heavy_synchro_lock);

    i = 0;
    start = take_time();
    while( i < heavy_synchro ) {
        if( rand() % 2 == 0 ) {
            e = dague_atomic_lifo_pop( &lifo1 );
            if(NULL != e) {
                DAGUE_LIST_ITEM_SINGLETON( e );
                dague_atomic_lifo_push(&lifo2, e);
                i++;
            }
        } else {
            e = dague_atomic_lifo_pop( &lifo2 );
            if(NULL != e) {
                DAGUE_LIST_ITEM_SINGLETON( e );
                dague_atomic_lifo_push(&lifo1, e);
                i++;
            }
        }
    }
    end = take_time();
    *p = diff_time(start, end);

    return NULL;
}

static void usage(const char *name, const char *msg)
{
    if( NULL != msg ) {
        fprintf(stderr, "%s\n", msg);
    }
    fprintf(stderr, 
            "Usage: \n"
            "   %s [-c cores|-n nbelt|-h|-?]\n"
            " where\n"
            "   -c cores:   cores (integer >0) defines the number of cores to test\n"
            "   -n nbelt:   nbelt (integer >0) defines the number of elements to use (default %u)\n"
            "   -N nbtimes: nbtimes (integer >0) defines the number of times elements must be moved from one list to another (default %u)\n",
            name,
            NBELT,
            NBTIMES);
    exit(1);
}

int main(int argc, char *argv[])
{
    unsigned int e;
    elt_t *elt, *p;
    pthread_t *threads;
    uint64_t *times;
    uint64_t min_time, max_time, sum_time;
    long int nbthreads = 1;
    int ch;
    char *m;
    
    while( (ch = getopt(argc, argv, "c:n:N:h?")) != -1 ) {
        switch(ch) {
        case 'c':
            nbthreads = strtol(optarg, &m, 0);
            if( (nbthreads <= 0) || (m[0] != '\0') ) {
                usage(argv[0], "invalid -c value");
            }
            break;
        case 'n':
            NBELT = strtol(optarg, &m, 0);
            if( (NBELT <= 0) || (m[0] != '\0') ) {
                usage(argv[0], "invalid -n value");
            }
            break;
        case 'N':
            NBTIMES = strtol(optarg, &m, 0);
            if( (NBTIMES <= 0) || (m[0] != '\0') ) {
                usage(argv[0], "invalid -N value");
            }
            break;
        case 'h':
        case '?':
        default:
            usage(argv[0], NULL);
            break;
        } 
    }

    threads = (pthread_t*)calloc(sizeof(pthread_t), nbthreads);
    times = (uint64_t*)calloc(sizeof(uint64_t), nbthreads);

    dague_atomic_lifo_construct( &lifo1 );
    dague_atomic_lifo_construct( &lifo2 );

    printf("Sequential test.\n");

    printf(" - create %u random elements and push them in lifo1\n", NBELT);
    for(e = 0; e < NBELT; e++) {
        elt = create_elem(e);
        dague_atomic_lifo_push( &lifo1, (dague_list_item_t *)elt );
    }

    check_translate_inorder(&lifo1, &lifo2, "lifo1", "lifo2");
    check_translate_inorder(&lifo2, &lifo1, "lifo2", "lifo1");

    printf("Parallel test.\n");

    printf(" - translate elements from lifo1 to lifo2 or from lifo2 to lifo1 (random), %u times on %ld threads\n",
           NBTIMES, nbthreads);
    for(e = 0; e < nbthreads; e++) {
        times[e] = e;
        pthread_create(&threads[e], NULL, translate_elements_random, &times[e]);
    }

    pthread_mutex_lock(&heavy_synchro_lock);
    heavy_synchro = NBTIMES;
    pthread_cond_broadcast(&heavy_synchro_cond);
    pthread_mutex_unlock(&heavy_synchro_lock);

    sum_time = 0;
    for(e = 0; e < nbthreads; e++) {
        pthread_join(threads[e], NULL);
        if( sum_time == 0 ) {
            min_time = times[e];
            max_time = times[e];
        } else {
            if( min_time > times[e] ) min_time = times[e];
            if( max_time < times[e] ) max_time = times[e];
        }
        sum_time += times[e];
    }
    printf("== Time to move %u times per thread for %ld threads from l1 to l2 or l2 to l1 randomly:\n"
           "== MIN %lu %s\n"
           "== MAX %lu %s\n"
           "== AVG %g %s\n",
           NBTIMES, nbthreads,
           min_time, TIMER_UNIT,
           max_time, TIMER_UNIT,
           (double)sum_time / (double)nbthreads, TIMER_UNIT);
    
    printf(" - move all elements to lifo1\n");
    p = NULL;
    ch = 0;
    while( !dague_atomic_lifo_is_empty( &lifo2 ) ) {
        elt = (elt_t*)dague_atomic_lifo_pop( &lifo2 );
        if( elt == NULL ) 
            fatal(" ! Error: list lifo2 is supposed to be non empty, but it is!\n");
        DAGUE_LIST_ITEM_SINGLETON( elt );
        if( elt == p ) 
            fatal(" ! I keep poping the same element in the list at element %u... It is now officially a frying pan\n",
                  ch);
        ch++;
        p = elt;
        dague_atomic_lifo_push( &lifo1, (dague_list_item_t*)elt );
    }
    
    check_translate_outoforder(&lifo1, &lifo2, "lifo1", "lifo2");
    
    printf(" - pop all elements from lifo1, and free them\n");
    while( !dague_atomic_lifo_is_empty( &lifo1 ) ) {
        elt = (elt_t*)dague_atomic_lifo_pop( &lifo1 );
        free(elt);
    }
    printf(" - pop all elements from lifo2, and free them\n");
    while( !dague_atomic_lifo_is_empty( &lifo2 ) ) {
        elt = (elt_t*)dague_atomic_lifo_pop( &lifo2 );
        free(elt);
    }

    free(threads);

    printf(" - all tests passed\n");

    return 0;
}
