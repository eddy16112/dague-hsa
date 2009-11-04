/*
 * Copyright (c) 2009      The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 */

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include "expr.h"
#include "symbol.h"

#define EXPR_EVAL_ERROR_SIZE   512
static char expr_eval_error[EXPR_EVAL_ERROR_SIZE];

static int expr_eval_unary(unsigned char op, const expr_t *op1,
                           const assignment_t *assignments, unsigned int nbassignments,
                           int *v)
{
    int rc;
    int v1;
    
    assert( EXPR_IS_UNARY(op) );

    rc = expr_eval(op1, assignments, nbassignments, &v1);
    if( EXPR_SUCCESS != rc ) {
        return rc;
    }

    switch(op) {
    case EXPR_OP_UNARY_NOT:
        *v = !v1;
        break;
    }

    return EXPR_SUCCESS;
}

static int expr_eval_binary(unsigned char op, const expr_t *op1, const expr_t *op2,
                            const assignment_t *assignments, unsigned int nbassignments,
                            int *v)
{
    int rc;
    int v1, v2;

    assert( EXPR_IS_BINARY(op) );

    rc = expr_eval(op1, assignments, nbassignments, &v1);
    if( EXPR_SUCCESS != rc ) {
        return rc;
    }
    rc = expr_eval(op2, assignments, nbassignments, &v2);
    if( EXPR_SUCCESS != rc ) {
        return rc;
    }

    switch(op) {
    case EXPR_OP_BINARY_MOD:
        *v = (v1 % v2);
        break;
    case EXPR_OP_BINARY_EQUAL:
        *v = (v1 == v2);
        break;
    case EXPR_OP_BINARY_NOT_EQUAL:
        *v = (v1 != v2);
        break;
    case EXPR_OP_BINARY_PLUS:
        *v = (v1 + v2);
        break;
    case EXPR_OP_BINARY_MINUS:
        *v = (v1 - v2);
        break;
    case EXPR_OP_BINARY_TIMES:
        *v = (v1 * v2);
        break;
    case EXPR_OP_BINARY_DIV:
        if( 0 == v2 ) {
            *v = INT_MAX;
        } else {
            *v = v1 / v2;
        }
        break;
    case EXPR_OP_BINARY_AND:
        if( (0 != v1) && (0 != v2) ) {
            *v = 1;
        } else {
            *v = 0;
        }
        break;
    case EXPR_OP_BINARY_OR:
        if( (0 != v1) || (0 != v2) ) {
            *v = 1;
        } else {
            *v = 0;
        }
        break;
    case EXPR_OP_BINARY_XOR:
        if( ((0 != v1) || (0 != v2)) && !((0 != v1) && (0 != v2)) ) {
            *v = 1;
        } else {
            *v = 0;
        }
        break;
    case EXPR_OP_BINARY_RANGE:
        snprintf(expr_eval_error, EXPR_EVAL_ERROR_SIZE, "Cannot evaluate range");
        return EXPR_FAILURE_CANNOT_EVALUATE_RANGE;
    }

    return EXPR_SUCCESS;
}

static int expr_eval_symbol(const symbol_t *sym, const assignment_t *assignments, unsigned int nbassignments, int *res)
{
    assignment_t* assignment;

    /* look at the global symbols first */
    const symbol_t *gsym = dplasma_search_global_symbol( sym->name );
    if( gsym != NULL ){
        int int_res;
        if( EXPR_SUCCESS == expr_eval((expr_t *)gsym->min, NULL, 0, &int_res) ){
            *res = int_res;
            return EXPR_SUCCESS;
        }
    }

    if( EXPR_SUCCESS == dplasma_find_assignment(sym->name, assignments, nbassignments, &assignment) ) {
        *res = assignment->value;
        return EXPR_SUCCESS;
    }

    snprintf(expr_eval_error, EXPR_EVAL_ERROR_SIZE, "Symbol not found in assignment: %s", sym->name);
    return EXPR_FAILURE_SYMBOL_NOT_FOUND;
}

int expr_eval(const expr_t *expr,
              const assignment_t *assignments, unsigned int nbassignments,
              int *res)
{
    /* If the expression was already evaluated to a constant, just return the
     * precalculated value.
     */
    if( EXPR_FLAG_CONSTANT & expr->flags ) {
        *res = expr->value;
        return EXPR_SUCCESS;
    }
    assert( EXPR_OP_CONST_INT != expr->op );

    if( EXPR_OP_SYMB == expr->op ) {
        int ret_val = expr_eval_symbol(expr->var, assignments, nbassignments, res);
        return ret_val;
    }
    if ( EXPR_IS_UNARY(expr->op) ) {
        return expr_eval_unary(expr->op, expr->uop1, assignments, nbassignments, res);
    }
    if ( EXPR_IS_BINARY(expr->op) ) {
        return expr_eval_binary(expr->op, expr->bop1, expr->bop2, assignments, nbassignments, res);
    }
    snprintf(expr_eval_error, EXPR_EVAL_ERROR_SIZE, "Unkown operand %d in expression", expr->op);
    return EXPR_FAILURE_UNKNOWN_OP;
}

int expr_parse_symbols( const expr_t* expr,
                        expr_symbol_check_callback_t* callback,
                        void* data )
{
    int rc;

    if( EXPR_OP_SYMB == expr->op ) {
        return callback(expr->var, data);
    }
    if( EXPR_OP_CONST_INT == expr->op ) {
        return EXPR_SUCCESS;
    }
    if( EXPR_IS_UNARY(expr->op) ) {
        return expr_parse_symbols( expr->uop1, callback, data );
    }
    rc = expr_parse_symbols( expr->bop1, callback, data );
    /* if we got an error don't check for the second expression */
    if( EXPR_SUCCESS == rc ) {
        return expr_parse_symbols( expr->bop2, callback, data );
    }
    return rc;
}

int expr_depend_on_symbol( const expr_t* expr,
                           const symbol_t* symbol )
{
    int rc;

    if( EXPR_OP_SYMB == expr->op ) {
        if( expr->var == symbol ) {
            return EXPR_SUCCESS;
        }
        return EXPR_FAILURE_SYMBOL_NOT_FOUND;
    }
    if( EXPR_OP_CONST_INT == expr->op ) {
        return EXPR_FAILURE_SYMBOL_NOT_FOUND;
    }
    if( EXPR_IS_UNARY(expr->op) ) {
        return expr_depend_on_symbol( expr->uop1, symbol );
    }
    rc = expr_depend_on_symbol( expr->bop1, symbol );
    if( EXPR_FAILURE_SYMBOL_NOT_FOUND == rc ) { /* not yet check for the second expression */
        return expr_depend_on_symbol( expr->bop2, symbol );
    }
    return rc;
}

#define EXPR_ABSOLUTE_RANGE_MIN 1
#define EXPR_ABSOLUTE_RANGE_MAX 2

int __expr_absolute_range_recursive( const expr_t* expr, int direction,
                                     int* pmin, int* pmax )
{
    int rc, *storage, lmin, lmax, rmin, rmax;

    if( EXPR_ABSOLUTE_RANGE_MIN == direction ) {
        storage = pmin;
    } else {
        assert( EXPR_ABSOLUTE_RANGE_MAX == direction );
        storage = pmax;
    }

    /* If the expression was already evaluated to a constant, just return the
     * precalculated value.
     */
    if( EXPR_FLAG_CONSTANT & expr->flags ) {
        *storage = expr->value;
        return EXPR_SUCCESS;
    }
    assert( EXPR_OP_CONST_INT != expr->op );

    if( EXPR_OP_SYMB == expr->op ) {
        const symbol_t* symbol = expr->var;
        const symbol_t* gsym = dplasma_search_global_symbol( symbol->name );
        if( gsym != NULL ) {
            if( EXPR_SUCCESS == expr_eval((expr_t *)gsym->min, NULL, 0, storage) ) {
                return EXPR_SUCCESS;
            }
            return EXPR_FAILURE_CANNOT_EVALUATE_RANGE;
        }
        if( EXPR_ABSOLUTE_RANGE_MIN == direction ) {
            rc = __expr_absolute_range_recursive( symbol->min, direction, pmin, pmax );
        } else {
            rc = __expr_absolute_range_recursive( symbol->max, direction, pmin, pmax );
        }
        return rc;
    }

    if( EXPR_IS_UNARY(expr->op) ) {
        /* there is no range for boolean values */
        return EXPR_FAILURE_CANNOT_EVALUATE_RANGE;
    }

    assert( EXPR_IS_BINARY(expr->op) );
    switch(expr->op) {
    case EXPR_OP_BINARY_MOD:
        printf( "No idea how to compute a min or max of a %%\n" );
        return EXPR_FAILURE_UNKNOWN;
    case EXPR_OP_BINARY_EQUAL:
    case EXPR_OP_BINARY_NOT_EQUAL:
        return EXPR_FAILURE_CANNOT_EVALUATE_RANGE;
    case EXPR_OP_BINARY_PLUS:
        rc = __expr_absolute_range_recursive( expr->bop1, direction, &lmin, &lmax );
        rc = __expr_absolute_range_recursive( expr->bop2, direction, &rmin, &rmax );
        if( EXPR_ABSOLUTE_RANGE_MIN == direction ) {
            *pmin = lmin + rmin;
        } else {
            *pmax = lmax + rmax;
        }
        return EXPR_SUCCESS;
    case EXPR_OP_BINARY_MINUS:
        rc = __expr_absolute_range_recursive( expr->bop1, direction, &lmin, &lmax );
        rc = __expr_absolute_range_recursive( expr->bop2, direction, &rmin, &rmax );
        if( EXPR_ABSOLUTE_RANGE_MIN == direction ) {
            *pmin = lmin - rmin;
        } else {
            *pmax = lmax - rmax;
        }
        return EXPR_SUCCESS;
    case EXPR_OP_BINARY_TIMES:
        rc = __expr_absolute_range_recursive( expr->bop1, direction, &lmin, &lmax );
        rc = __expr_absolute_range_recursive( expr->bop2, direction, &rmin, &rmax );
        if( EXPR_ABSOLUTE_RANGE_MIN == direction ) {
            *pmin = lmin * rmin;
        } else {
            *pmax = lmax * rmax;
        }
        return EXPR_SUCCESS;
    case EXPR_OP_BINARY_DIV:
    case EXPR_OP_BINARY_AND:
    case EXPR_OP_BINARY_OR:
    case EXPR_OP_BINARY_XOR:
    case EXPR_OP_BINARY_RANGE:
        /* should we continue down the expressions of the range ? */
        return EXPR_FAILURE_CANNOT_EVALUATE_RANGE;
    }
    return EXPR_FAILURE_CANNOT_EVALUATE_RANGE;
}

int expr_absolute_range(const expr_t* expr,
                        int* pmin, int* pmax)
{
    int rc, unused;

    rc = __expr_absolute_range_recursive( expr, EXPR_ABSOLUTE_RANGE_MIN, pmin, &unused );
    if( EXPR_SUCCESS != rc ) {
        return rc;
    }
    rc = __expr_absolute_range_recursive( expr, EXPR_ABSOLUTE_RANGE_MAX, &unused, pmax );
    if( EXPR_SUCCESS != rc ) {
        return rc;
    }

    return EXPR_SUCCESS;
}

int expr_range_to_min_max(const expr_t *expr,
                          const assignment_t *assignments, unsigned int nbassignments,
                          int *min, int *max)
{
    int rc;

    assert( expr->op == EXPR_OP_BINARY_RANGE );

    rc = expr_eval(expr->bop1, assignments, nbassignments, min);
    if( EXPR_SUCCESS != rc ) {
        return rc;
    }
    rc = expr_eval(expr->bop2, assignments, nbassignments, max);
    if( EXPR_SUCCESS != rc ) {
        return rc;
    }

    return EXPR_SUCCESS;
}

expr_t *expr_new_var(const symbol_t *symb)
{
    expr_t *r = (expr_t*)calloc(1, sizeof(expr_t));
    r->op = EXPR_OP_SYMB;
    r->var = (symbol_t*)symb;
    if( dplasma_symbol_is_global(symb) ) {
        r->flags = EXPR_FLAG_CONSTANT;
        r->value = symb->min->value;
    } else {
        r->flags = 0;  /* no flags */
    }
    return r;
}

expr_t *expr_new_int(int v)
{
    expr_t *r = (expr_t*)calloc(1, sizeof(expr_t));
    r->op    = EXPR_OP_CONST_INT;
    r->flags = EXPR_FLAG_CONSTANT;
    r->value = v;
    return r;
}

/* This function should negate an expression */
expr_t *expr_new_unary(char op, expr_t *e)
{
    expr_t *n = (expr_t*)calloc(1, sizeof(expr_t));
    n->uop1 = e;
    n->op   = EXPR_OP_UNARY_NOT;
    if( e->flags & EXPR_FLAG_CONSTANT ) {
        n->flags = EXPR_FLAG_CONSTANT;
        n->value = !(n->value);
    } else {
        n->flags = 0;  /* unknown yet */
    }
    return n;
}

expr_t *expr_new_binary(const expr_t *op1, char op, const expr_t *op2)
{
    expr_t *r = (expr_t*)calloc(1, sizeof(expr_t));
    int is_constant;

    r->bop1  = (expr_t*)op1;
    r->bop2  = (expr_t*)op2;
    r->flags = 0;  /* unknown yet */
    is_constant = op1->flags & op2->flags & EXPR_FLAG_CONSTANT;

    switch( op ) {
    case '+':
        r->op = EXPR_OP_BINARY_PLUS;
        if( is_constant ) {
            r->flags = EXPR_FLAG_CONSTANT;
            r->value = op1->value + op2->value;
        }
        return r;
    case '-':
        r->op = EXPR_OP_BINARY_MINUS;
        if( is_constant ) {
            r->flags = EXPR_FLAG_CONSTANT;
            r->value = op1->value - op2->value;
        }
        return r;
    case '*':
        r->op = EXPR_OP_BINARY_TIMES;
        if( is_constant ) {
            r->flags = EXPR_FLAG_CONSTANT;
            r->value = op1->value * op2->value;
        }
        return r;
    case '%':
        r->op = EXPR_OP_BINARY_MOD;
        if( is_constant ) {
            r->flags = EXPR_FLAG_CONSTANT;
            r->value = op1->value % op2->value;
        }
        return r;
    case '=':
        r->op = EXPR_OP_BINARY_EQUAL;
        if( is_constant ) {
            r->flags = EXPR_FLAG_CONSTANT;
            r->value = op1->value && op2->value;
        }
        return r;
    case '!':
        r->op = EXPR_OP_BINARY_NOT_EQUAL;
        if( is_constant ) {
            r->flags = EXPR_FLAG_CONSTANT;
            r->value = !(op1->value && op2->value);
        }
        return r;
    case '.':
        r->op = EXPR_OP_BINARY_RANGE;
        return r;
    case '/':
        r->op = EXPR_OP_BINARY_DIV;
        return r;
    case '|':
        r->op = EXPR_OP_BINARY_OR;
        return r;
    case '&':
        r->op = EXPR_OP_BINARY_AND;
        return r;
    case '^':
        r->op = EXPR_OP_BINARY_XOR;
        return r;
    default:
        free(r);
        fprintf(stderr, "Unknown operand %c. Return NULL expression\n", op );
        return NULL;
    }

    return r;
}

char *expr_error(void)
{
    return expr_eval_error;
}

static void expr_dump_unary(unsigned char op, const expr_t *op1)
{
    switch(op) {
    case EXPR_OP_UNARY_NOT:
        printf("!(");
        break;
    }

    if( NULL == op1 ) {
        printf("NULL");
    } else {
        expr_dump(op1);
        if( op == EXPR_OP_UNARY_NOT ){
            printf(")");
        }
    }
}

static void expr_dump_binary(unsigned char op, const expr_t *op1, const expr_t *op2)
{
    if( EXPR_OP_BINARY_RANGE == op ) {
        printf( " [" );
        expr_dump(op1);
        printf( " .. " );
        expr_dump(op2);
        printf( "] " );
        return;
    }

    if( EXPR_OP_BINARY_EQUAL == op ) {
        printf( " (" );
        expr_dump(op1);
        printf( " == " );
        expr_dump(op2);
        printf( ") " );
        return;
    }

    if( EXPR_OP_BINARY_NOT_EQUAL == op ) {
        printf( " (" );
        expr_dump(op1);
        printf( " != " );
        expr_dump(op2);
        printf( ") " );
        return;
    }

    expr_dump(op1);

    switch( op ) {
    case EXPR_OP_BINARY_PLUS:
        printf(" + ");
        break;
    case EXPR_OP_BINARY_MINUS:
        printf(" - ");
        break;
    case EXPR_OP_BINARY_TIMES:
        printf(" * ");
        break;
    case EXPR_OP_BINARY_MOD:
        printf(" %% ");
        break;
    case EXPR_OP_BINARY_DIV:
        printf(" / ");
        break;
    case EXPR_OP_BINARY_AND:
        printf(" & ");
        break;
    case EXPR_OP_BINARY_OR:
        printf(" | ");
        break;
    case EXPR_OP_BINARY_XOR:
        printf(" ^ ");
        break;
    }

    expr_dump(op2);
}

void expr_dump(const expr_t *e)
{
    if( NULL == e ) {
        printf("NULL");
        return;
    }
    if( EXPR_FLAG_CONSTANT & e->flags ) {
        if( EXPR_OP_CONST_INT == e->op ) {
            printf( "%d", e->value );
            return;
        }
        printf( "{%d:", e->value );
    }
    if( EXPR_OP_SYMB == e->op ) {
        if( dplasma_symbol_is_global(e->var) ) {
            printf("%s", e->var->name);
        } else {
            int res;
            if( EXPR_SUCCESS == expr_eval_symbol(e->var, NULL, 0, &res)){
                printf("%d", res);
            }else{
                printf("%s", e->var->name);
            }
        }
    } else if( EXPR_OP_CONST_INT == e->op ) {
        printf("%d", e->value);
    } else if( EXPR_IS_UNARY(e->op) ) {
        expr_dump_unary(e->op, e->uop1);
    } else if( EXPR_IS_BINARY(e->op) ) {
        expr_dump_binary(e->op, e->bop1, e->bop2);
    } else {
        fprintf(stderr, "Unkown operand %d in expression", e->op);
    }
    if( EXPR_FLAG_CONSTANT & e->flags ) {
        printf( "}" );
    }
}

char *dump_c_expression(FILE *out, const expr_t *e, const char *prefix)
{
    static unsigned int expr_idx = 0;
    static char name[64];

    if( e == NULL ) {
        sprintf(name, "NULL");
    } else {
        int my_id = expr_idx;
        expr_idx++;

        if( EXPR_OP_CONST_INT == e->op ) {
            fprintf(out, "static expr_t expr%d = { .op = EXPR_OP_CONST_INT, .flags = %d, .value = %d };\n",
                    my_id, e->flags, e->value);
        } 
        else if( EXPR_OP_SYMB == e->op ) {
            fprintf(out, "static expr_t expr%d = { .op = EXPR_OP_SYMB, .flags = %d, .var = &dplasma_symbols[%d] };\n",
                    my_id, e->flags, symbol_c_index_lookup( e->var ));
        } else if( EXPR_IS_UNARY(e->op) ) {
            char sn[64];
            sprintf(sn, "%s", dump_c_expression(out, e->uop1, prefix));
            fprintf(out, "static expr_t expr%d = { .op = %d, .flags = %d, .uop1 = %s };\n", 
                    my_id, e->op, e->flags, sn);
        } else if( EXPR_IS_BINARY(e->op) ) {
            char sn1[64];
            char sn2[64];
            sprintf(sn1, "%s", dump_c_expression(out, e->bop1, prefix));
            sprintf(sn2, "%s", dump_c_expression(out, e->bop2, prefix));
            fprintf(out, "static expr_t expr%d = { .op = %d, .flags = %d, .bop1 = %s, .bop2 = %s };\n", 
                    my_id, e->op, e->flags, sn1, sn2);
        } else {
            fprintf(stderr, "Unkown operand %d in expression", e->op);
        }

        sprintf(name, "&expr%d", my_id);
    }

    return name;
}
