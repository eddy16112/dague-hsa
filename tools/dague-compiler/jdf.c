#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "jdf.h"

jdf_t current_jdf;
int current_lineno;

extern const char *yyfilename;

void jdf_warn(int lineno, const char *format, ...)
{
    char msg[512];
    va_list ap;

    va_start(ap, format);
    vsnprintf(msg, 512, format, ap);
    va_end(ap);

    fprintf(stderr, "Warning on %s:%d: %s", yyfilename, lineno, msg);
}

void jdf_fatal(int lineno, const char *format, ...)
{
    char msg[512];
    va_list ap;

    va_start(ap, format);
    vsnprintf(msg, 512, format, ap);
    va_end(ap);

    fprintf(stderr, "Fatal Error on %s:%d: %s", yyfilename, lineno, msg);
}

void jdf_prepare_parsing(void)
{
    current_jdf.prologue  = NULL;
    current_jdf.epilogue  = NULL;
    current_jdf.globals   = NULL;
    current_jdf.functions = NULL;
    current_lineno = 1;
}

static int jdf_sanity_check_global_redefinitions(void)
{
    jdf_global_entry_t *g1, *g2;
    int rc = 0;

    for(g1 = current_jdf.globals; g1 != NULL; g1 = g1->next) {
        for(g2 = g1->next; g2 != NULL; g2 = g2->next) {
            if( !strcmp(g1->name, g2->name) ) {
                jdf_fatal(g2->lineno, "Global %s is redefined here (previous definition was on line %d)\n",
                          g1->name, g1->lineno);
                rc = -1;
            }
        }
    }
    return rc;
}

static int jdf_sanity_check_global_masked(void)
{
    jdf_global_entry_t *g;
    jdf_function_entry_t *f;
    jdf_name_list_t *n;
    jdf_def_list_t *d;
    int rc = 0;

    for(g = current_jdf.globals; g != NULL; g = g->next) {
        for(f = current_jdf.functions; f != NULL; f = f->next) {
            for(n = f->parameters; n != NULL; n = n->next) {
                if( !strcmp(n->name, g->name) ) {
                    jdf_warn(f->lineno, "Global %s defined line %d is masked by the local parameter %s of function %s\n",
                             g->name, g->lineno, n->name, f->fname);
                    rc++;
                }
            }
            for(d = f->definitions; d != NULL; d = d->next) {
                if( !strcmp(d->name, g->name) ) {
                    jdf_warn(d->lineno, "Global %s defined line %d is masked by the local definition of %s in function %s\n",
                             g->name, g->lineno, d->name, f->fname);
                    rc++;
                }
            }
        }
    }
    return rc;
}

static int jdf_sanity_check_expr_bound_before_global(jdf_expr_t *e, jdf_global_entry_t *g1)
{
    jdf_global_entry_t *g2;
    int rc = 0;
    switch( e->op ) {
    case JDF_VAR:
        for(g2 = current_jdf.globals; g2 != g1; g2 = g2->next) {
            if( !strcmp( e->jdf_var, g2->name ) ) {
                break;
            }
        }
        if( g2 == g1 ) {
            jdf_fatal(g1->lineno, "Global %s is defined using variable %s which is unbound at this time\n",
                      g1->name, e->jdf_var);
            rc = -1;
        }
        return rc;
    case JDF_CST:
        return 0;
    case JDF_TERNARY:
        if( jdf_sanity_check_expr_bound_before_global(e->jdf_tat, g1) < 0 )
            rc = -1;
        if( jdf_sanity_check_expr_bound_before_global(e->jdf_ta1, g1) < 0 )
            rc = -1;
        if( jdf_sanity_check_expr_bound_before_global(e->jdf_ta2, g1) < 0 )
            rc = -1;
        return rc;
    case JDF_NOT:
        if( jdf_sanity_check_expr_bound_before_global(e->jdf_ua, g1) < 0 )
            rc = -1;
        return rc;
    default:
        if( jdf_sanity_check_expr_bound_before_global(e->jdf_ba1, g1) < 0 )
            rc = -1;
        if( jdf_sanity_check_expr_bound_before_global(e->jdf_ba2, g1) < 0 )
            rc = 1;
        return rc;
    }
}

static int jdf_sanity_check_global_unbound(void)
{
    int rc = 0;
    jdf_global_entry_t *g;
    for(g = current_jdf.globals; g != NULL; g = g->next) {
        if( NULL != g->expression ) {
            if( jdf_sanity_check_expr_bound_before_global(g->expression, g) < 0 )
                rc = -1;
        }
    }
    return rc;
}

static int jdf_sanity_check_function_redefinitions(void)
{
    jdf_function_entry_t *f1, *f2;
    int rc = 0;

    for(f1 = current_jdf.functions; f1 != NULL; f1 = f1->next) {
        for(f2 = f1->next; f2 != NULL; f2 = f2->next) {
            if( !strcmp(f1->fname, f2->fname) ) {
                jdf_fatal(f2->lineno, "Function %s is redefined here (previous definition was on line %d)\n",
                          f1->fname, f1->lineno);
                rc = -1;
            }
        }
    }
    return rc;    
}

static int jdf_sanity_check_all_parameters_defined(void)
{
    jdf_function_entry_t *f;
    jdf_name_list_t *p;
    jdf_def_list_t *d;
    int rc = 0;

    for(f = current_jdf.functions; f != NULL; f = f->next) {
        for(p = f->parameters; p != NULL; p = p->next) {
            for(d = f->definitions; d != NULL; d = d->next) {
                if( !strcmp(d->name, p->name) ) {
                    break;
                }
            }
            if( d == NULL ) {
                jdf_fatal(f->lineno, "Parameter %s of function %s is declared but no range is associated to it\n",
                          p->name, f->fname);
                rc = -1;
            }
        }
    }
    return rc;
}

static int jdf_sanity_check_expr_bound_before_definition(jdf_expr_t *e, jdf_function_entry_t *f, jdf_def_list_t *d)
{
    jdf_global_entry_t *g;
    jdf_def_list_t *d2;
    int rc = 0;
    switch( e->op ) {
    case JDF_VAR:
        for(g = current_jdf.globals; g != NULL; g = g->next) {
            if( !strcmp( e->jdf_var, g->name ) ) {
                break;
            }
        }
        if( g == NULL ) {
            for(d2 = f->definitions; d2 != d; d2 = d2->next) {
                if( !strcmp( e->jdf_var, d2->name ) ) {
                    break;
                }
            }
            if( d2 == d ) {
                jdf_fatal(d->lineno, "Local %s is defined using variable %s which is unbound at this time\n",
                          d->name, e->jdf_var);
                rc = -1;
            }
        }
        return rc;
    case JDF_CST:
        return 0;
    case JDF_TERNARY:
        if( jdf_sanity_check_expr_bound_before_definition(e->jdf_tat, f, d) < 0 )
            rc = -1;
        if( jdf_sanity_check_expr_bound_before_definition(e->jdf_ta1, f, d) < 0 )
            rc = -1;
        if( jdf_sanity_check_expr_bound_before_definition(e->jdf_ta2, f, d) < 0 )
            rc = -1;
        return rc;
    case JDF_NOT:
        if( jdf_sanity_check_expr_bound_before_definition(e->jdf_ua, f, d) < 0 )
            rc = -1;
        return rc;
    default:
        if( jdf_sanity_check_expr_bound_before_definition(e->jdf_ba1, f, d) < 0 )
            rc = -1;
        if( jdf_sanity_check_expr_bound_before_definition(e->jdf_ba2, f, d) < 0 )
            rc = 1;
        return rc;
    }
}

static int jdf_sanity_check_definition_unbound(void)
{
    int rc = 0;
    jdf_function_entry_t *f;
    jdf_def_list_t *d;

    for(f = current_jdf.functions; f != NULL; f = f->next) {
        for(d = f->definitions; d != NULL; d = d->next) {
            if( jdf_sanity_check_expr_bound_before_definition(d->expr, f, d) < 0 )
                rc = -1;
        }
    }
    return rc;
}

static int jdf_sanity_check_expr_bound(jdf_expr_t *e, const char *kind, jdf_function_entry_t *f)
{
    jdf_global_entry_t *g;
    jdf_def_list_t *d;
    int rc = 0;
    switch( e->op ) {
    case JDF_VAR:
        for(g = current_jdf.globals; g != NULL; g = g->next) {
            if( !strcmp( e->jdf_var, g->name ) ) {
                break;
            }
        }
        if( g == NULL ) {
            for(d = f->definitions; d != NULL; d = d->next) {
                if( !strcmp( e->jdf_var, d->name ) ) {
                    break;
                }
            }
            if( d == NULL ) {
                jdf_fatal(f->lineno, "%s of function %s is defined using variable %s which is unbound at this time\n",
                          kind, f->fname, e->jdf_var);
                rc = -1;
            }
        }
        return rc;
    case JDF_CST:
        return 0;
    case JDF_TERNARY:
        if( jdf_sanity_check_expr_bound(e->jdf_tat, kind, f) < 0 )
            rc = -1;
        if( jdf_sanity_check_expr_bound(e->jdf_ta1, kind, f) < 0 )
            rc = -1;
        if( jdf_sanity_check_expr_bound(e->jdf_ta2, kind, f) < 0 )
            rc = -1;
        return rc;
    case JDF_NOT:
        if( jdf_sanity_check_expr_bound(e->jdf_ua, kind, f) < 0 )
            rc = -1;
        return rc;
    default:
        if( jdf_sanity_check_expr_bound(e->jdf_ba1, kind, f) < 0 )
            rc = -1;
        if( jdf_sanity_check_expr_bound(e->jdf_ba2, kind, f) < 0 )
            rc = 1;
        return rc;
    }
}

static int jdf_sanity_check_predicates_unbound(void)
{
    int rc = 0;
    jdf_function_entry_t *f;
    jdf_expr_list_t *e;
    int i;
    char kind[64];

    for(f = current_jdf.functions; f != NULL; f = f->next) {
        i = 0;
        for(e = f->predicate->parameters; e != NULL; e = e->next) {
            snprintf(kind, 64, "Parameter number %d of predicate", i);
            if( jdf_sanity_check_expr_bound(e->expr, kind, f) < 0 )
                rc = -1;
            i++;
        }
    }
    return rc;
}

static int jdf_sanity_check_dataflow_expressions_unbound(void)
{
    int rc = 0;
    jdf_function_entry_t *f;
    jdf_dataflow_list_t *d;
    jdf_expr_list_t *e;
    jdf_dep_list_t *dep;
    int i, j, k;
    char kind[128];

    for(f = current_jdf.functions; f != NULL; f = f->next) {
        i = 1;
        for(d = f->dataflow; d != NULL; d = d->next) {
            j =  1;
            for(dep = d->flow->deps; dep != NULL; dep = dep->next) {
                snprintf(kind, 128, 
                         "Guard of dependency %d\n"
                         "  of dataflow number %d (variable %s) at line %d", 
                         j, i,  d->flow->varname, d->flow->lineno);
                if( (dep->dep->guard->guard_type != JDF_GUARD_UNCONDITIONAL) && 
                    (jdf_sanity_check_expr_bound(dep->dep->guard->guard, kind, f) < 0) )
                    rc = -1;
                k = 1;
                for(e = dep->dep->guard->calltrue->parameters; e != NULL; e = e->next) {
                    snprintf(kind, 128, 
                             "Parameter %d of dependency %d\n"
                             "  of dataflow number %d (variable %s) at line %d", 
                             k, j, i, d->flow->varname, d->flow->lineno);
                    if( jdf_sanity_check_expr_bound(e->expr, kind, f) < 0 )
                        rc = -1;
                    k++;
                }
                if( dep->dep->guard->guard_type == JDF_GUARD_TERNARY ) {
                    k = 1;
                    for(e = dep->dep->guard->callfalse->parameters; e != NULL; e = e->next) {
                        snprintf(kind, 128, 
                                 "Parameter %d of dependency %d (when guard false)\n"
                                 "  of dataflow number %d (variable %s) at line %d", 
                                 k, j, i,  d->flow->varname, d->flow->lineno);
                        if( jdf_sanity_check_expr_bound(e->expr, kind, f) < 0 )
                            rc = -1;
                        k++;
                    }
                }
                j++;
            }
            i++;
        }
    }
    return rc;
}

static int jdf_sanity_check_dataflow_naming_collisions(void)
{
    int rc = 0;
    jdf_function_entry_t *f1, *f2;
    jdf_dataflow_list_t *d;
    jdf_dep_list_t *dep;

    for(f1 = current_jdf.functions; f1 != NULL; f1 = f1->next) {
        for(f2 = current_jdf.functions; f2 != NULL; f2 = f2->next) {
            for(d = f2->dataflow; d != NULL; d = d->next) {
                for(dep = d->flow->deps; dep != NULL; dep = dep->next) {
                    if( !strcmp(dep->dep->guard->calltrue->func_or_mem, f1->fname) &&
                        (dep->dep->guard->calltrue->var == NULL) ) {
                        jdf_fatal(dep->dep->lineno, 
                                  "%s is the name of a function (defined line %d):\n"
                                  "  it cannot be also used as a memory reference in function %s\n",
                                  f1->fname, f1->lineno, f2->fname);
                        rc = -1;
                    }
                    if( dep->dep->guard->guard_type == JDF_GUARD_TERNARY &&
                        !strcmp(dep->dep->guard->callfalse->func_or_mem, f1->fname) &&
                        (dep->dep->guard->callfalse->var == NULL) ) {
                        jdf_fatal(dep->dep->lineno, 
                                  "%s is the name of a function (defined line %d):\n"
                                  "  it cannot be also used as a memory reference in function %s\n",
                                  f1->fname, f1->lineno, f2->fname);
                        rc = -1;
                    }
                }
            }
        }
    }
    return rc;
}

static int jdf_sanity_check_dataflow_unexisting_data(void)
{
    int rc = 0;
    jdf_function_entry_t *f1, *f2;
    jdf_dataflow_list_t *d1, *d2;
    jdf_dep_list_t *dep;
    int i, j;

    for(f1 = current_jdf.functions; f1 != NULL; f1 = f1->next) {
        i = 1;
        for(d1 = f1->dataflow; d1 != NULL; d1 = d1->next) {
            j = 1;
            for(dep = d1->flow->deps; dep != NULL; dep = dep->next) {

                if( (dep->dep->guard->calltrue->var != NULL) ) {
                    for(f2 = current_jdf.functions; f2 != NULL; f2 = f2->next) {
                        if( !strcmp(f2->fname, dep->dep->guard->calltrue->func_or_mem) ) {
                            /* found the function, let's find the data */
                            for(d2 = f2->dataflow; d2 != NULL; d2 = d2->next) {
                                if( !strcmp(d2->flow->varname, dep->dep->guard->calltrue->var) ) {
                                    break;
                                }
                            }
                            if( d2 == NULL ) {
                                jdf_fatal(dep->dep->lineno, 
                                          "Function %s has no data named %s,\n"
                                          "  but dependency %d of dataflow %d of function %s (variable %s) references it\n",
                                          f2->fname, dep->dep->guard->calltrue->var, 
                                          j, i, f1->fname, d1->flow->varname);
                                rc = -1;
                            }
                            break;
                        }
                    }
                    if( f2 == NULL ) {
                        jdf_fatal(dep->dep->lineno, 
                                  "There is no function name %s,\n"
                                          "  but dependency %d of dataflow %d of function %s (variable %s) references it\n",
                                  dep->dep->guard->calltrue->func_or_mem, 
                                  j, i, f1->fname, d1->flow->varname);
                        rc = -1;
                    }
                }

                if( (dep->dep->guard->guard_type == JDF_GUARD_TERNARY) && 
                    (dep->dep->guard->callfalse->var != NULL) ) {
                    for(f2 = current_jdf.functions; f2 != NULL; f2 = f2->next) {
                        if( !strcmp(f2->fname, dep->dep->guard->callfalse->func_or_mem) ) {
                            /* found the function, let's find the data */
                            for(d2 = f2->dataflow; d2 != NULL; d2 = d2->next) {
                                if( !strcmp(d2->flow->varname, dep->dep->guard->callfalse->var) ) {
                                    break;
                                }
                            }
                            if( d2 == NULL ) {
                                jdf_fatal(dep->dep->lineno, 
                                          "Function %s has no data named %s,\n"
                                          "  but then part of dependency %d of dataflow %d of function %s (variable %s) references it\n",
                                          f2->fname, dep->dep->guard->callfalse->var, 
                                          j, i, f1->fname, d1->flow->varname);
                                rc = -1;
                            }
                            break;
                        }
                    }
                    if( f2 == NULL ) {
                        jdf_fatal(dep->dep->lineno, 
                                  "There is no function name %s,\n"
                                          "  but then part of dependency %d of dataflow %d of function %s (variable %s) references it\n",
                                  dep->dep->guard->callfalse->func_or_mem, 
                                  j, i, f1->fname, d1->flow->varname);
                        rc = -1;
                    }
                }                

                j++;
            }

            i++;
        }
    }

    return rc;
}

int jdf_sanity_checks( jdf_warning_mask_t mask )
{
    int rc = 0;
    int fatal = 0;
    int rcsum = 0;

#define DO_CHECK( call ) do {                       \
    rc = call;                                      \
    if(rc < 0)                                      \
        fatal = 1;                                  \
    else                                            \
        rcsum += rc;                                \
    } while(0)

    DO_CHECK( jdf_sanity_check_global_redefinitions() );
    DO_CHECK( jdf_sanity_check_global_unbound() );
    if( mask & JDF_WARN_MASKED_GLOBALS )
        DO_CHECK( jdf_sanity_check_global_masked() );

    DO_CHECK( jdf_sanity_check_function_redefinitions() );
    DO_CHECK( jdf_sanity_check_all_parameters_defined() );
    DO_CHECK( jdf_sanity_check_definition_unbound() );

    DO_CHECK( jdf_sanity_check_predicates_unbound() );
    DO_CHECK( jdf_sanity_check_dataflow_expressions_unbound() );

    DO_CHECK( jdf_sanity_check_dataflow_naming_collisions() );
    DO_CHECK( jdf_sanity_check_dataflow_unexisting_data() );

#undef DO_CHECK

    if(fatal)
        return -1;
    return rcsum;
}
