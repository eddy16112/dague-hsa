/*
 * Copyright (c) 2009      The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "dplasma.h"

static const dplasma_t** dplasma_array = NULL;
static int dplasma_array_size = 0, dplasma_array_count = 0;
static int global_execution = 1;

void dplasma_dump(const dplasma_t *d, const char *prefix)
{
    int i;
    char *pref2 = malloc(strlen(prefix)+3);

    sprintf(pref2, "%s  ", prefix);
    printf("%sDplasma Function: %s\n", prefix, d->name);

    printf("%s Parameter Variables:\n", prefix);
    for(i = 0; i < MAX_LOCAL_COUNT && NULL != d->locals[i]; i++) {
        symbol_dump(d->locals[i], pref2);
    }

    printf("%s Predicates:\n", prefix);
    for(i = 0; i < MAX_PRED_COUNT && NULL != d->preds[i]; i++) {
        printf("%s", pref2);
        expr_dump(d->preds[i]);
        printf("\n");
    }

    printf("%s Parameters and Dependencies:\n", prefix);
    for(i = 0; i < MAX_PARAM_COUNT && NULL != d->params[i]; i++) {
        param_dump(d->params[i], pref2);
    }

    printf("%s Body:\n", prefix);
    printf("%s  %s\n", prefix, d->body);

    free(pref2);
}

void dplasma_dump_all( void )
{
    int i;

    for( i = 0; i < dplasma_array_count; i++ ) {
        printf("/**\n * dplasma_t object named %s index %d\n */\n", dplasma_array[i]->name, i );
        dplasma_dump( dplasma_array[i], "" );
    }
}

int dplasma_push( const dplasma_t* d )
{
    if( dplasma_array_count >= dplasma_array_size ) {
        if( 0 == dplasma_array_size ) {
            dplasma_array_size = 4;
        } else {
            dplasma_array_size *= 2;
        }
        dplasma_array = (const dplasma_t**)realloc( dplasma_array, dplasma_array_size * sizeof(dplasma_t*) );
        if( NULL == dplasma_array ) {
            return -1;  /* No more available memory */
        }
    }
    dplasma_array[dplasma_array_count] = d;
    dplasma_array_count++;
    return 0;
}

const dplasma_t* dplasma_find( const char* name )
{
    int i;
    const dplasma_t* object;

    for( i = 0; i < dplasma_array_count; i++ ) {
        object = dplasma_array[i];
        if( 0 == strcmp( object->name, name ) ) {
            return object;
        }
    }
    return NULL;
}

dplasma_t* dplasma_find_or_create( const char* name )
{
    dplasma_t* object;

    object = (dplasma_t*)dplasma_find(name);
    if( NULL != object ) {
        return object;
    }
    object = (dplasma_t*)calloc(1, sizeof(dplasma_t));
    object->name = strdup(name);
    if( 0 == dplasma_push(object) ) {
        return object;
    }
    free(object);
    return NULL;
}

const dplasma_t* dplasma_element_at( int i )
{
    if( i < dplasma_array_count ){
        return dplasma_array[i];
    }
    return NULL;
}

/* There is another loop after this one. */
#define DPLASMA_LOOP_NEXT       0x01
/* This is the final loop */
#define DPLASMA_LOOP_FINAL      0x02
/* This loops array is allocated */
#define DPLASMA_LOOP_ALLOCATED  0x04
typedef struct dplasma_final_loop_values_t {
    char array[1];
} dplasma_final_loop_values_t;

typedef struct dplasma_loop_values_t dplasma_loop_values_t;
typedef union dplasma_loop_union_t {
    dplasma_loop_values_t* loops[1];
    dplasma_final_loop_values_t* array[1];
} dplasma_loop_union_t;

struct dplasma_loop_values_t {
    int                   type;
    symbol_t*             symbol;
    dplasma_loop_union_t* generic_next;
    int                   min;
    int                   max;
    dplasma_loop_union_t  u;
};

dplasma_loop_values_t*
dplasma_dependency_create_loop( int type,
                                symbol_t* symbol,
                                int min,
                                int max )
{
    dplasma_loop_values_t* loops = (dplasma_loop_values_t*)malloc(sizeof(dplasma_loop_values_t));

    loops->type = type;
    loops->symbol = symbol;
    loops->generic_next = NULL;
    loops->min = min;
    loops->max = max;

    return loops;
}

typedef struct dplasma_execution_context_t {
    dplasma_t* function;
    assignment_t locals[MAX_LOCAL_COUNT];
    dplasma_loop_values_t* loops;
} dplasma_execution_context_t;

int dplasma_dependency_activate( dplasma_execution_context_t* context, int arg_index, ... )
{
    return 0;
}

#if 0
#define DEBUG(ARG)  printf ARG
#else
#define DEBUG(ARG)
#endif

int dplasma_set_initial_execution_context( dplasma_execution_context_t* exec_context )
{
    int i, nb_locals, rc;
    const dplasma_t* object = exec_context->function;
    const expr_t** predicates;

    /* Compute the number of local values */
    for( i = nb_locals = 0; (NULL != object->locals[i]) && (i < MAX_LOCAL_COUNT); i++, nb_locals++ );
    if( 0 == nb_locals ) {
        /* special case for the IN/OUT obejcts */
        return 0;
    }

    /**
     * This section of the code generate all possible executions for a specific
     * dplasma object. If the global execution is true (!= 0) then it generate
     * the instances globally, otherwise it will generate them locally (using
     * the predicates).
     */
    predicates = NULL;
    if( !global_execution ) {
        predicates = (const expr_t**)object->preds;
    }

    for( i = 0; i < nb_locals; i++ ) {
        int min;
        exec_context->locals[i].sym = object->locals[i];
        rc = dplasma_symbol_get_first_value(object->locals[i], predicates,
                                            exec_context->locals, &min);
        if( rc != EXPR_SUCCESS ) {
        initial_values_one_loop_up:
            i--;
            if( i < 0 ) {
                printf( "Impossible to find initial values. Giving up\n" );
                return -1;
            }
            rc = dplasma_symbol_get_next_value(object->locals[i], predicates,
                                               exec_context->locals, &min );
            if( rc != EXPR_SUCCESS ) {
                goto initial_values_one_loop_up;
            }
        }
    }
    return 0;
}

int plasma_show_ranges( const dplasma_t* object )
{
    dplasma_execution_context_t* exec_context = (dplasma_execution_context_t*)malloc(sizeof(dplasma_execution_context_t));
    dplasma_loop_values_t* last_loop = NULL;
    const expr_t** predicates;
    int i, nb_locals;

    exec_context->function = (dplasma_t*)object;

    /* Compute the number of local values */
    for( i = nb_locals = 0; (NULL != object->locals[i]) && (i < MAX_LOCAL_COUNT); i++, nb_locals++ );
    if( 0 == nb_locals ) {
        /* special case for the IN/OUT obejcts */
        return 0;
    }
    printf( "Function %s (loops %d)\n", object->name, nb_locals );

    /**
     * If the global execution is true (!= 0) then it generate
     * the values globally, otherwise it will generate them locally (using
     * the predicates).
     */
    predicates = NULL;
    if( !global_execution ) {
        predicates = (const expr_t**)object->preds;
    }

    /**
     * This section of code walk through the tree and printout the local and global
     * minimum and maximum values for all local variables.
     */
    for( i = 0; i < nb_locals; i++ ) {
        dplasma_loop_values_t* loop;
        int abs_min, min, abs_max, max;
        exec_context->locals[i].sym = object->locals[i];
        dplasma_symbol_get_first_value(object->locals[i], predicates,
                                       exec_context->locals, &min);
        exec_context->locals[i].value = min;
        dplasma_symbol_get_last_value(object->locals[i], predicates,
                                      exec_context->locals, &max);

        dplasma_symbol_get_absolute_minimum_value(object->locals[i], &abs_min);
        dplasma_symbol_get_absolute_maximum_value(object->locals[i], &abs_max);

        printf( "Range for local symbol %s is [%d..%d] (global range [%d..%d]) %s\n",
                object->locals[i]->name, min, max, abs_min, abs_max,
                (0 == dplasma_symbol_is_standalone(object->locals[i]) ? "[standalone]" : "[dependent]") );

        loop = dplasma_dependency_create_loop( DPLASMA_LOOP_FINAL, exec_context->locals[i].sym, min, max );
        if( NULL != last_loop ) {
            last_loop->type = DPLASMA_LOOP_NEXT;
            last_loop->generic_next = (dplasma_loop_union_t*)loop;
        } else {
            exec_context->loops = loop;
        }
        last_loop = loop;
    }
    return 0;
}

int dplasma_show_tasks( const dplasma_t* object )
{
    dplasma_execution_context_t* exec_context = (dplasma_execution_context_t*)malloc(sizeof(dplasma_execution_context_t));
    int i, nb_locals, rc, actual_loop;
    const expr_t** predicates;

    exec_context->function = (dplasma_t*)object;

    /* Compute the number of local values */
    for( i = nb_locals = 0; (NULL != object->locals[i]) && (i < MAX_LOCAL_COUNT); i++, nb_locals++ );
    if( 0 == nb_locals ) {
        /* special case for the IN/OUT obejcts */
        return 0;
    }
    printf( "Function %s (loops %d)\n", object->name, nb_locals );

    /**
     * This section of the code generate all possible executions for a specific
     * dplasma object. If the global execution is true (!= 0) then it generate
     * the instances globally, otherwise it will generate them locally (using
     * the predicates).
     */
    predicates = NULL;
    if( !global_execution ) {
        predicates = (const expr_t**)object->preds;
    }

    if( 0 != dplasma_set_initial_execution_context(exec_context) ) {
        /* if we can't initialize the execution context then there is no reason to
         * continue.
         */
        return -1;
    }

    actual_loop = nb_locals - 1;
    while(1) {
        int value;

        /* Do whatever we have to do for this context */
        printf( "Execute %s with ", object->name );
        for( i = 0; i <= actual_loop; i++ ) {
            printf( "(%s = %d)", object->locals[i]->name,
                    exec_context->locals[i].value );
        }
        printf( "\n" );

        /* Go to the next valid value for this loop context */
        rc = dplasma_symbol_get_next_value( object->locals[actual_loop], predicates,
                                            exec_context->locals, &value );

        /* If no more valid values, go to the previous loop,
         * compute the next valid value and redo and reinitialize all other loops.
         */
        if( rc != EXPR_SUCCESS ) {
            int current_loop = actual_loop;
        one_loop_up:
            DEBUG(("Loop index %d based on %s failed to get next value. Going up ...\n",
                   actual_loop, object->locals[actual_loop]->name));
            if( 0 == actual_loop ) {  /* we're done */
                goto end_of_all_loops;
            }
            actual_loop--;  /* one level up */
            rc = dplasma_symbol_get_next_value( object->locals[actual_loop], predicates,
                                                exec_context->locals, &value );
            if( rc != EXPR_SUCCESS ) {
                goto one_loop_up;
            }
            DEBUG(("Keep going on the loop level %d (symbol %s value %d)\n", actual_loop,
                   object->locals[actual_loop]->name, exec_context->locals[actual_loop].value));
            for( actual_loop++; actual_loop <= current_loop; actual_loop++ ) {
                rc = dplasma_symbol_get_first_value(object->locals[actual_loop], predicates,
                                                    exec_context->locals, &value );
                if( rc != EXPR_SUCCESS ) {  /* no values for this symbol in this context */
                    goto one_loop_up;
                }
                DEBUG(("Loop index %d based on %s get first value %d\n", actual_loop,
                       object->locals[actual_loop]->name, exec_context->locals[actual_loop].value));
            }
            actual_loop = current_loop;  /* go back to the original loop */
        } else {
            DEBUG(("Loop index %d based on %s get next value %d\n", actual_loop,
                   object->locals[actual_loop]->name, exec_context->locals[actual_loop].value));
        }
    }
 end_of_all_loops:

    return 0;
}

int dplasma_complete_execution( dplasma_execution_context_t* exec_context )
{
    dplasma_t* function = exec_context->function;
    param_t* param;
    dep_t* dep;
    int i, j, k, rc, value;

    for( i = 0; (i < MAX_PARAM_COUNT) && (NULL != function->params[i]); i++ ) {
        param = function->params[i];

        if( !(SYM_OUT & param->sym_type) ) {
            continue;  /* this is only an INPUT dependency */
        }
        for( j = 0; (j < MAX_DEP_OUT_COUNT) && (NULL != param->dep_out[j]); j++ ) {
            int dont_generate = 0;

            dep = param->dep_out[j];
            if( NULL != dep->cond ) {
                /* Check if the condition apply on the current setting */
                rc = expr_eval( dep->cond, exec_context->locals, MAX_LOCAL_COUNT, &value );
                if( 0 == value ) {
                    continue;
                }
            }
            /* Check to see if any of the params are conditionals or ranges and if they are
             * if they match.
             */
            for( k = 0; (k < MAX_CALL_PARAM_COUNT) && (NULL != dep->call_params[k]); k++ ) {
                if( EXPR_OP_BINARY_RANGE == dep->call_params[k]->op ) {
                    int min, max;
                    rc = expr_range_to_min_max( dep->call_params[k], exec_context->locals, MAX_LOCAL_COUNT, &min, &max );
                    if( min > max ) {
                        dont_generate = 1;
                    }
                }
            }
            if( dont_generate ) {
                continue;
            }

            printf( "-> %s of %s( ", dep->sym_name, dep->dplasma->name );
            for( k = 0; (k < MAX_CALL_PARAM_COUNT) && (NULL != dep->call_params[k]); k++ ) {
                if( EXPR_OP_BINARY_RANGE != dep->call_params[k]->op ) {
                    rc = expr_eval( dep->call_params[k], exec_context->locals, MAX_LOCAL_COUNT, &value );
                    printf( "%d ", value );
                } else {
                    int min, max;
                    rc = expr_range_to_min_max( dep->call_params[k], exec_context->locals, MAX_LOCAL_COUNT, &min, &max );
                    if( min == max ) {
                        printf( "%d ", min );
                    } else {
                        printf( "[%d..%d] ", min, max );
                    }
                }
            }
            printf( ")\n" );
        }
    }

    return 0;
}

int dplasma_unroll( const dplasma_t* object )
{
    dplasma_execution_context_t* exec_context = (dplasma_execution_context_t*)malloc(sizeof(dplasma_execution_context_t));
    int i, nb_locals, rc, actual_loop;
    const expr_t** predicates;

    exec_context->function = (dplasma_t*)object;

    /* Compute the number of local values */
    for( i = nb_locals = 0; (NULL != object->locals[i]) && (i < MAX_LOCAL_COUNT); i++, nb_locals++ );
    if( 0 == nb_locals ) {
        /* special case for the IN/OUT obejcts */
        return 0;
    }
    printf( "Function %s (loops %d)\n", object->name, nb_locals );

    /**
     * This section of the code generate all possible executions for a specific
     * dplasma object. If the global execution is true (!= 0) then it generate
     * the instances globally, otherwise it will generate them locally (using
     * the predicates).
     */
    predicates = NULL;
    if( !global_execution ) {
        predicates = (const expr_t**)object->preds;
    }

    if( 0 != dplasma_set_initial_execution_context(exec_context) ) {
        /* if we can't initialize the execution context then there is no reason to
         * continue.
         */
        return -1;
    }

    actual_loop = nb_locals - 1;
    while(1) {
        int value;

        /* Do whatever we have to do for this context */
        printf( "Execute %s with ", object->name );
        for( i = 0; i <= actual_loop; i++ ) {
            printf( "(%s = %d)", object->locals[i]->name,
                    exec_context->locals[i].value );
        }
        printf( "\n" );

        /* Complete the execution of this service and release the
         * dependencies.
         */
        dplasma_complete_execution( exec_context );

        /* Go to the next valid value for this loop context */
        rc = dplasma_symbol_get_next_value( object->locals[actual_loop], predicates,
                                            exec_context->locals, &value );

        /* If no more valid values, go to the previous loop,
         * compute the next valid value and redo and reinitialize all other loops.
         */
        if( rc != EXPR_SUCCESS ) {
            int current_loop = actual_loop;
        one_loop_up:
            DEBUG(("Loop index %d based on %s failed to get next value. Going up ...\n",
                   actual_loop, object->locals[actual_loop]->name));
            if( 0 == actual_loop ) {  /* we're done */
                goto end_of_all_loops;
            }
            actual_loop--;  /* one level up */
            rc = dplasma_symbol_get_next_value( object->locals[actual_loop], predicates,
                                                exec_context->locals, &value );
            if( rc != EXPR_SUCCESS ) {
                goto one_loop_up;
            }
            DEBUG(("Keep going on the loop level %d (symbol %s value %d)\n", actual_loop,
                   object->locals[actual_loop]->name, exec_context->locals[actual_loop].value));
            for( actual_loop++; actual_loop <= current_loop; actual_loop++ ) {
                rc = dplasma_symbol_get_first_value(object->locals[actual_loop], predicates,
                                                    exec_context->locals, &value );
                if( rc != EXPR_SUCCESS ) {  /* no values for this symbol in this context */
                    goto one_loop_up;
                }
                DEBUG(("Loop index %d based on %s get first value %d\n", actual_loop,
                       object->locals[actual_loop]->name, exec_context->locals[actual_loop].value));
            }
            actual_loop = current_loop;  /* go back to the original loop */
        } else {
            DEBUG(("Loop index %d based on %s get next value %d\n", actual_loop,
                   object->locals[actual_loop]->name, exec_context->locals[actual_loop].value));
        }
    }
 end_of_all_loops:

    return 0;
}
