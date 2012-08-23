/*
 * Copyright (c) 2009-2010 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 */

#ifndef _DA_UTILITY_H_
#define _DA_UTILITY_H_
#include "dague_config.h"
#include <stdlib.h>
#include <stdarg.h>
#include "node_struct.h"
#include "string_arena.h"

BEGIN_C_DECLS

typedef struct _var_t var_t;
typedef struct _und_t und_t;
typedef struct _dep_t dep_t;
typedef struct _expr_t expr_t;

struct _und_t{
    int rw;
    int type;
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

char *indent(int n, int size);
void jdfoutput(const char *format, ...);    

// AST utility functions
int    DA_is_if(node_t *node);
int    DA_is_loop(node_t *node);
int    DA_is_scf(node_t *node);
int    DA_is_rel(node_t *node);
int    DA_flip_rel_op(int type);
int    DA_canonicalize_for(node_t *node);
void   DA_parentize(node_t *node);
char   *DA_type_name(node_t *node);
char   *DA_var_name(node_t *node);
node_t *DA_array_base(node_t *node);
node_t *DA_array_index(node_t *node, int i);
int    DA_array_dim_count(node_t *node);
node_t *DA_loop_induction_variable(node_t *loop);
node_t *DA_loop_lb(node_t *node);
node_t *DA_loop_ub(node_t *node);
node_t *DA_if_condition(node_t *node);
node_t *DA_if_then_body(node_t *node);
node_t *DA_if_else_body(node_t *node);
node_t *DA_exp_to_ind(node_t *node);
int     DA_exp_to_const(node_t *node);

node_t *DA_create_ID(char *name);
node_t *DA_create_Int_const(int64_t val);
node_t *DA_create_B_expr(int type, node_t *kid0, node_t *kid1);
node_t *DA_create_Unary(uint32_t type);
node_t *DA_create_Block(void);
node_t *DA_create_For(node_t *scond, node_t *econd, node_t *incr, node_t *body);
node_t *DA_create_Complex(uint32_t type, char *arrayName, ...);
void DA_insert_first(node_t *block, node_t *new_node);
void DA_insert_last(node_t *block, node_t *new_node);
node_t *DA_create_Entry();
node_t *DA_create_Exit();

int DA_tree_contains_only_known_vars(node_t *node, char **known_vars);
#define DA_create_relation(_T_, _K0_, _K1_) DA_create_B_expr(_T_, _K0_, _K1_)
#define DA_create_ArrayAccess(name, ...) DA_create_Complex(ARRAY, name, __VA_ARGS__)
#define DA_create_Fcall(name, ...) DA_create_Complex(FCALL, name, __VA_ARGS__)

void convert_OUTPUT_to_INOUT(node_t *node);
void add_entry_and_exit_task_loops(node_t *node);

char *quark_tree_to_body(node_t *node);
node_t *quark_get_locality(node_t *node);
string_arena_t *create_pool_declarations(void);


// yacc utility
node_t *node_to_ptr(node_t node);

// Use/Def data structure utility functions
und_t **get_variable_uses_and_defs(node_t *node);
void add_variable_use_or_def(node_t *node, int rw, int type, int task_count);
void rename_induction_variables(node_t *node);

// Analysis
void analyze_deps(node_t *node);
void assign_UnD_to_tasks(node_t *node);

// Debug and symbolic reconstruction (unparse) functions
char *append_to_string(char *str, const char *app, const char *fmt, size_t add_length);
char *tree_to_str(node_t *node);
char *tree_to_str_with_substitutions(node_t *node, str_pair_t *solved_vars);
const char *type_to_symbol(int type);
void dump_tree(node_t node, int offset);
void dump_for(node_t *node);
void dump_all_unds(void);
void dump_und(und_t *und);

#define DA_kid(_N_, _X_)   ((_N_)->u.kids.kids[(_X_)])
#define DA_kid_count(_N_)   ((_N_)->u.kids.kid_count)
#define DA_assgn_lhs(_N_)  DA_kid((_N_), 0)
#define DA_assgn_rhs(_N_)  DA_kid((_N_), 1)
#define DA_rel_lhs(_N_)    DA_kid((_N_), 0)
#define DA_rel_rhs(_N_)    DA_kid((_N_), 1)
#define DA_exp_lhs(_N_)    DA_kid((_N_), 0)
#define DA_exp_rhs(_N_)    DA_kid((_N_), 1)
#define DA_for_body(_N_)   DA_kid((_N_), 3)
#define DA_for_scond(_N_)  DA_kid((_N_), 0)
#define DA_for_econd(_N_)  DA_kid((_N_), 1)
#define DA_for_modifier(_N_)  DA_kid((_N_), 2)
#define DA_while_cond(_N_) DA_kid((_N_), 0)
#define DA_while_body(_N_) DA_kid((_N_), 1)
#define DA_do_cond(_N_)    DA_kid((_N_), 0)
#define DA_do_body(_N_)    DA_kid((_N_), 1)
#define DA_func_name(_N_)  (( (NULL!=(_N_)) && (DA_kid((_N_), 0)->type == IDENTIFIER) ) ? DA_kid((_N_), 0)->u.var_name : NULL)
#define DA_var_name(_N_)   (( (NULL!=(_N_)) && ((_N_)->type == IDENTIFIER) ) ? (_N_)->u.var_name : NULL)
#define DA_int_val(_N_)    ((_N_)->const_val.i64_value)


#define UND_IGNORE 0x0
#define UND_READ   0x1
#define UND_WRITE  0x2
#define UND_RW     0x3
#define is_und_read(_U_)   ((_U_ ->rw) & 0x1)
#define is_und_write(_U_) (((_U_ ->rw) & 0x2)>>1)

END_C_DECLS

#endif
