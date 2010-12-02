/*
 * Copyright (c) 2009-2010 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 */

#ifndef _DA_UTILITY_H_
#define _DA_UTILITY_H_
#include "dague_config.h"
#include <stdlib.h>
#include "node_struct.h"

BEGIN_C_DECLS

typedef struct _var_t var_t;
typedef struct _und_t und_t;
typedef struct _dep_t dep_t;
typedef struct _expr_t expr_t;

struct _und_t{
    int rw;
    int task_num;
    node_t *node;
    und_t *next;
};

struct _var_t{
    char *var_name;
    und_t *und;
    var_t *next;
};

struct _expr_t{
    int type;
    expr_t *l;
    expr_t *r;
    union {
        const char *name;
        long int int_const;
    } value;
};
    

// AST utility functions
int    DA_is_loop(node_t *node);
int    DA_is_scf(node_t *node);
int    DA_is_rel(node_t *node);
int    DA_flip_rel_op(int type);
int    DA_canonicalize_for(node_t *node);
void   DA_parentize(node_t node);
char   *DA_type_name(node_t *node);
char   *DA_var_name(node_t *node);
node_t *DA_array_base(node_t *node);
node_t *DA_array_index(node_t *node, int i);
int    DA_array_dim_count(node_t *node);
node_t *DA_loop_induction_variable(node_t *loop);
node_t *DA_loop_lb(node_t *node);
node_t *DA_loop_ub(node_t *node);
node_t *DA_create_int_const(int64_t val);
node_t *DA_create_B_expr(int type, node_t *kid0, node_t *kid1);
node_t *DA_create_Entry();
node_t *DA_create_Exit();
#define DA_create_relation(_T_, _K0_, _K1_) DA_create_B_expr(_T_, _K0_, _K1_)

char *quark_tree_to_body(node_t *node);

// yacc utility
node_t *node_to_ptr(node_t node);

// Use/Def data structure utility functions
und_t **get_variable_uses_and_defs(node_t *node);
void add_variable_use_or_def(node_t *node, int rw, int task_count);
void rename_induction_variables(node_t *node);


// Analysis
void analyze_deps(node_t *node);
void assign_UnD_to_tasks(node_t *node);

// Debug and symbolic reconstruction (unparse) functions
char *append_to_string(char *str, const char *app, const char *fmt, size_t add_length);
char *tree_to_str(node_t *node);
const char *type_to_symbol(int type);
void dump_tree(node_t node, int offset);
void dump_for(node_t *node);
void dump_all_unds(void);
void dump_und(und_t *und);

#define DA_kid(_N_, _X_)   ((_N_)->u.kids.kids[(_X_)])
#define DA_assgn_lhs(_N_)  DA_kid((_N_), 0)
#define DA_assgn_rhs(_N_)  DA_kid((_N_), 1)
#define DA_rel_lhs(_N_)    DA_kid((_N_), 0)
#define DA_rel_rhs(_N_)    DA_kid((_N_), 1)
#define DA_for_body(_N_)   DA_kid((_N_), 3)
#define DA_for_scond(_N_)  DA_kid((_N_), 0)
#define DA_for_econd(_N_)  DA_kid((_N_), 1)
#define DA_for_incrm(_N_)  DA_kid((_N_), 2)
#define DA_while_cond(_N_) DA_kid((_N_), 0)
#define DA_while_body(_N_) DA_kid((_N_), 1)
#define DA_do_cond(_N_)    DA_kid((_N_), 0)
#define DA_do_body(_N_)    DA_kid((_N_), 1)
#define DA_func_name(_N_)  (( DA_kid((_N_), 0)->type == IDENTIFIER ) ? DA_kid((_N_), 0)->u.var_name : NULL)
#define DA_int_val(_N_)    ((_N_)->const_val.i64_value)


#define UND_IGNORE 0x0
#define UND_READ   0x1
#define UND_WRITE  0x2
#define UND_RW     0x3
#define is_und_read(_U_)   ((_U_ ->rw) & 0x1)
#define is_und_write(_U_) (((_U_ ->rw) & 0x2)>>1)

END_C_DECLS

#endif
