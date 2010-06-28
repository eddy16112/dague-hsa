#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <math.h>

#include "jdf.h"
#include "string_arena.h"
#include "jdf2c_utils.h"

extern const char *yyfilename;

static FILE *cfile;
static int   cfile_lineno;
static FILE *hfile;
static int   hfile_lineno;
static const char *jdf_basename;
static const char *jdf_cfilename;

/** Optional declarations of local functions */
static int jdf_expr_depends_on_symbol(const char *varname, const jdf_expr_t *expr);
static void jdf_generate_code_hook(const jdf_t *jdf, const jdf_function_entry_t *f, const char *fname);
static void jdf_generate_code_release_deps(const jdf_t *jdf, const jdf_function_entry_t *f, const char *fname);
static void jdf_generate_code_iterate_successors(const jdf_t *jdf, const jdf_function_entry_t *f, const char *prefix);

/** A coutput and houtput functions to write in the .h and .c files, counting the number of lines */

static int nblines(const char *p)
{
    int r = 0;
    for(; *p != '\0'; p++)
        if( *p == '\n' )
            r++;
    return r;
}

#if defined(__GNUC__)
static void coutput(const char *format, ...) __attribute__((format(printf,1,2)));
#endif
static void coutput(const char *format, ...)
{
    va_list ap;
    char *res;
    int len;

    va_start(ap, format);
    len = vasprintf(&res, format, ap);
    va_end(ap);

    if( len == -1 ) {
        fprintf(stderr, "Unable to ouptut a string: %s\n", strerror(errno));
    } else {
        fwrite(res, len, 1, cfile);
        cfile_lineno += nblines(res);
        free(res);
    }
}

#if defined(__GNUC__)
static void houtput(const char *format, ...) __attribute__((format(printf,1,2)));
#endif
static void houtput(const char *format, ...)
{
    va_list ap;
    char *res;
    int len;

    va_start(ap, format);
    len = vasprintf(&res, format, ap);
    va_end(ap);

    if( len == -1 ) {
        fprintf(stderr, "Unable to ouptut a string: %s\n", strerror(errno));
    } else {
        fwrite(res, len, 1, hfile);
        hfile_lineno += nblines(res);
        free(res);
    }
}

/** Utils: dumper functions for UTIL_DUMP_LIST_FIELD and UTIL_DUMP_LIST calls **/

/**
 * dump_string:
 *  general function to use with UTIL_DUMP_LIST_FIELD.
 *  Transforms a single field pointing to an existing char * in the char *
 * @param [IN] elt: pointer to the char * (format useable by UTIL_DUMP_LIST_FIELD)
 * @param [IN] _:   ignored pointer to abide by UTIL_DUMP_LIST_FIELD format
 * @return the char * pointed by elt
 */
static char *dump_string(void **elt, void *_)
{
    return (char*)*elt;
}

/**
 * dump_globals:
 *   Dump a global symbol like #define ABC (__dague_object->ABC)
 */
static char* dump_globals(void** elem, void *arg)
{
    string_arena_t *sa = (string_arena_t*)arg;

    string_arena_init(sa);
    string_arena_add_string(sa, "%s (__dague_object->super.%s)", (char*)*elem, (char*)*elem );
    return string_arena_get_string(sa);
}

/**
 * dump_data:
 *   Dump a global symbol like
 *     #define ABC(A0, A1) (__dague_object->ABC->data_of(__dague_object->ABC, A0, A1))
 */
static char* dump_data(void** elem, void *arg)
{
    jdf_data_entry_t* data = (jdf_data_entry_t*)elem;
    string_arena_t *sa = (string_arena_t*)arg;
    int i, len = 0;

    string_arena_init(sa);
    string_arena_add_string(sa, "%s(%s%d", data->dname, data->dname, 0 );
    for( i = 1; i < data->nbparams; i++ ) {
        string_arena_add_string(sa, ",%s%d", data->dname, i );
    }
    string_arena_add_string(sa, ")  (__dague_object->super.%s->data_of(__dague_object->super.%s", 
                            data->dname, data->dname);
    for( i = 0; i < data->nbparams; i++ ) {
        string_arena_add_string(sa, ", (%s%d)", data->dname, i );
    }
    string_arena_add_string(sa, "))\n" );
    return string_arena_get_string(sa);
}

/**
 * Parameters for dump_expr helper
 */
typedef struct expr_info {
    string_arena_t* sa;
    const char* prefix;
} expr_info_t;

/**
 * dump_expr:
 *   dumps the jdf_expr* pointed to by elem into arg->sa, prefixing each 
 *   non-global variable with arg->prefix
 */
static char * dump_expr(void **elem, void *arg)
{
    expr_info_t* expr_info = (expr_info_t*)arg;
    expr_info_t li, ri;
    jdf_expr_t *e = *(jdf_expr_t**)elem;
    string_arena_t *sa = expr_info->sa;
    string_arena_t *la, *ra;

    string_arena_init(sa);

    la = string_arena_new(8);
    ra = string_arena_new(8);

    li.sa = la;
    li.prefix = expr_info->prefix;
    
    ri.sa = ra;
    ri.prefix = expr_info->prefix;

    switch( e->op ) {
    case JDF_VAR: {
        jdf_global_entry_t* item = current_jdf.globals;
        while( item != NULL ) {
            if( !strcmp(item->name, e->jdf_var) ) {
                string_arena_add_string(sa, "%s", e->jdf_var);
                break;
            }
            item = item->next;
        }
        if( NULL == item ) {
            string_arena_add_string(sa, "%s%s", expr_info->prefix, e->jdf_var);
        }
        break;
    }
    case JDF_EQUAL:
        string_arena_add_string(sa, "(%s == %s)", 
                                dump_expr((void**)&e->jdf_ba1, &li), 
                                dump_expr((void**)&e->jdf_ba2, &ri) );
        break;
    case JDF_NOTEQUAL:
        string_arena_add_string(sa, "(%s != %s)", 
                                dump_expr((void**)&e->jdf_ba1, &li), 
                                dump_expr((void**)&e->jdf_ba2, &ri) );
        break;
    case JDF_AND:
        string_arena_add_string(sa, "(%s && %s)",
                                dump_expr((void**)&e->jdf_ba1, &li),
                                dump_expr((void**)&e->jdf_ba2, &ri) );
        break;
    case JDF_OR:
        string_arena_add_string(sa, "(%s || %s)",
                                dump_expr((void**)&e->jdf_ba1, &li),
                                dump_expr((void**)&e->jdf_ba2, &ri) );
        break;
    case JDF_XOR:
        string_arena_add_string(sa, "(%s ^ %s)",
                                dump_expr((void**)&e->jdf_ba1, &li),
                                dump_expr((void**)&e->jdf_ba2, &ri) );
        break;
    case JDF_LESS:
        string_arena_add_string(sa, "(%s <  %s)",
                                dump_expr((void**)&e->jdf_ba1, &li),
                                dump_expr((void**)&e->jdf_ba2, &ri) );
        break;
    case JDF_LEQ:
        string_arena_add_string(sa, "(%s <= %s)",
                                dump_expr((void**)&e->jdf_ba1, &li),
                                dump_expr((void**)&e->jdf_ba2, &ri) );
        break;
    case JDF_MORE:
        string_arena_add_string(sa, "(%s >  %s)",
                                dump_expr((void**)&e->jdf_ba1, &li),
                                dump_expr((void**)&e->jdf_ba2, &ri) );
        break;
    case JDF_MEQ:
        string_arena_add_string(sa, "(%s >= %s)",
                                dump_expr((void**)&e->jdf_ba1, &li),
                                dump_expr((void**)&e->jdf_ba2, &ri) );
        break;
    case JDF_NOT:
        string_arena_add_string(sa, "!%s",
                                dump_expr((void**)&e->jdf_ua, &li));
        break;
    case JDF_PLUS:
        string_arena_add_string(sa, "(%s + %s)",
                                dump_expr((void**)&e->jdf_ba1, &li),
                                dump_expr((void**)&e->jdf_ba2, &ri) );
        break;
    case JDF_MINUS:
        string_arena_add_string(sa, "(%s - %s)",
                                dump_expr((void**)&e->jdf_ba1, &li),
                                dump_expr((void**)&e->jdf_ba2, &ri) );
        break;
    case JDF_TIMES:
        string_arena_add_string(sa, "(%s * %s)",
                                dump_expr((void**)&e->jdf_ba1, &li),
                                dump_expr((void**)&e->jdf_ba2, &ri) );
        break;
    case JDF_DIV:
        string_arena_add_string(sa, "(%s / %s)",
                                dump_expr((void**)&e->jdf_ba1, &li),
                                dump_expr((void**)&e->jdf_ba2, &ri) );
        break;
    case JDF_MODULO:
        string_arena_add_string(sa, "(%s %% %s)",
                                dump_expr((void**)&e->jdf_ba1, &li),
                                dump_expr((void**)&e->jdf_ba2, &ri) );
        break;
    case JDF_SHL:
        string_arena_add_string(sa, "(%s << %s)",
                                dump_expr((void**)&e->jdf_ba1, &li),
                                dump_expr((void**)&e->jdf_ba2, &ri) );
        break;
    case JDF_SHR:
        string_arena_add_string(sa, "(%s >> %s)",
                                dump_expr((void**)&e->jdf_ba1, &li),
                                dump_expr((void**)&e->jdf_ba2, &ri) );
        break;
    case JDF_RANGE:
        break;
    case JDF_TERNARY: {
        expr_info_t ti;
        string_arena_t *ta;
        ta = string_arena_new(8);
        ti.sa = ta;
        ti.prefix = expr_info->prefix;

        string_arena_add_string(sa, "(%s ? %s : %s)",
                                dump_expr((void**)&e->jdf_tat, &ti),
                                dump_expr((void**)&e->jdf_ta1, &li),
                                dump_expr((void**)&e->jdf_ta2, &ri) );

        string_arena_free(ta);
        break;
    }
    case JDF_CST:
        string_arena_add_string(sa, "%d", e->jdf_cst);
        break;
    default:
        string_arena_add_string(sa, "DonKnow");
        break;
    }
    string_arena_free(la);
    string_arena_free(ra);
    
    return string_arena_get_string(sa);
}

/**
 * Dump a predicate like 
 *  #define F_pred(k, n, m) (__dague_object->ABC->rank == __dague_object->ABC->rank_of(__dague_object->ABC, k, n, m))
 */
static char* dump_predicate(void** elem, void *arg)
{
    jdf_function_entry_t *f = (jdf_function_entry_t *)elem;
    string_arena_t *sa = (string_arena_t*)arg;
    string_arena_t *sa2 = string_arena_new(64);
    string_arena_t *sa3 = string_arena_new(64);
    expr_info_t expr_info;

    jdf_call_t* call = f->predicate;
    int i, len = 0;

    string_arena_init(sa);
    string_arena_add_string(sa, "%s_pred(%s) ",
                            f->fname,
                            UTIL_DUMP_LIST_FIELD(sa2, f->parameters, next, name,
                                                 dump_string, NULL, 
                                                 "", "", ", ", ""));
    expr_info.sa = sa3;
    expr_info.prefix = "";
    string_arena_add_string(sa, "(__dague_object->super.%s->myrank == __dague_object->super.%s->rank_of(__dague_object->super.%s, %s))", 
                            f->predicate->func_or_mem, f->predicate->func_or_mem, f->predicate->func_or_mem,
                            UTIL_DUMP_LIST_FIELD(sa2, f->predicate->parameters, next, expr,
                                                 dump_expr, &expr_info,
                                                 "", "", ", ", "")); 

    string_arena_free(sa2);
    string_arena_free(sa3);
    return string_arena_get_string(sa);
}

/**
 * Dump a repository line, like
 * #define F_repo (__dague_object->F_repo)
 */
static char *dump_repo(void **elem, void *arg)
{
    jdf_function_entry_t *f = (jdf_function_entry_t *)elem;
    string_arena_t *sa = (string_arena_t*)arg;

    string_arena_init(sa);
    string_arena_add_string(sa, "%s_repo (__dague_object->%s_repository)",
                            f->fname, f->fname);

    return string_arena_get_string(sa);
}

/**
 * Parameters of the dump_assignments function
 */
typedef struct assignment_info {
    string_arena_t *sa;
    int idx;
    const char *holder;
    const jdf_expr_t *expr;
} assignment_info_t;

/**
 * dump_assignments:
 *  Takes the pointer to the name of a parameter,
 *  a pointer to a dump_info, and prints <assignment_info.holder>k = assignments[<assignment_info.idx>] 
 *  into assignment_info.sa for each variable k that belong to the expression that is going
 *  to be used. This expression is passed into assignment_info->expr. If assignment_info->expr 
 *  is NULL, all variables are assigned.
 */
static char *dump_assignments(void **elem, void *arg)
{
    char *varname = *(char**)elem;
    assignment_info_t *info = (assignment_info_t*)arg;
    
    string_arena_init(info->sa);
    if( (NULL == info->expr) || jdf_expr_depends_on_symbol(varname, info->expr) ) {
        string_arena_add_string(info->sa, "%s = %s[%d].value;\n", varname, info->holder, info->idx);
        info->idx++;
        return string_arena_get_string(info->sa);
    } else {
        info->idx++;
        return NULL;
    }
}

/**
 * dump_dataflow:
 *  Takes the pointer to a jdf_flow, and a pointer to either "IN" or "OUT",
 *  and print the name of the variable for this flow if it's a variable as IN or as OUT
 *  NULL otherwise (and the UTIL_DUMP_LIST_FIELD macro will jump above these elements)
 */
static char *dump_dataflow(void **elem, void *arg)
{
    jdf_dataflow_t *d = *(jdf_dataflow_t**)elem;
    jdf_dep_type_t target;
    jdf_dep_list_t *l;

    if( !strcmp(arg, "IN") )
        target = JDF_DEP_TYPE_IN;
    else
        target = JDF_DEP_TYPE_OUT;

    for(l = d->deps; l != NULL; l = l->next)
        if( l->dep->type == target )
            break;
    if( l == NULL )
        return NULL;
    return d->varname;
}

/**
 * dump_resinit:
 *  Takes the pointer to a char *, which is a parameter name of the _new function,
 *  and a string_arena, and writes in the string_arena the assignment of this
 *  parameter in the res structure to the parameter passed to the _new function.
 */
static char *dump_resinit(void **elem, void *arg)
{
    string_arena_t *sa = (string_arena_t*)arg;
    char *varname = *(char**)elem;
    string_arena_init(sa);

    string_arena_add_string(sa, "res->super.%s = %s;", varname, varname);

    return string_arena_get_string(sa);
}

/**
 * dump_data_declaration:
 *  Takes the pointer to a flow *f, let say that f->varname == "A",
 *  this produces a string like void *A = NULL;\n  
 *  gc_data_t *gT = NULL;\n  data_repo_entry_t *eT = NULL;\n
 */
static char *dump_data_declaration(void **elem, void *arg)
{
    string_arena_t *sa = (string_arena_t*)arg;
    jdf_dataflow_t *f = *(jdf_dataflow_t**)elem;
    char *varname = f->varname;

    string_arena_init(sa);

    string_arena_add_string(sa, 
                            "  void *%s = NULL;\n"
                            "  gc_data_t *g%s = NULL;\n"
                            "  data_repo_entry_t *e%s = NULL;\n", 
                            varname, varname, varname);

    return string_arena_get_string(sa);
}

/**
 * dump_dataflow_varname:
 *  Takes the pointer to a flow *f, and print the varname
 */
static char *dump_dataflow_varname(void **elem, void *_)
{
    jdf_dataflow_t *f = *(jdf_dataflow_t **)elem;
    return f->varname;
}

static double unique_rgb_color_saturation;
static double unique_rgb_color_value;

static void init_unique_rgb_color(void)
{
    unique_rgb_color_value = 0.5 + (0.5 * (double)rand() / (double)RAND_MAX);
    unique_rgb_color_saturation = 0.5 + (0.5 * (double)rand() / (double)RAND_MAX);
}

static void get_unique_rgb_color(float ratio, unsigned char *r, unsigned char *g, unsigned char *b)
{
    double h, s, v, r1, g1, b1;

    h = ratio;
    s = 0.8;
    v = 0.8;

    if ( s == 0.0 ) {
        *r = (unsigned char)(v * 255);
        *g = (unsigned char)(v * 255);
        *b = (unsigned char)(v * 255);
    } else {
        double var_h = (h == 1.0) ? 0.0 : h * 6.0;
        int var_i = (int)floor(var_h);
        float var_1 = (v * ( 1.0 - s ));
        float var_2 = (v * ( 1.0 - s * ( var_h - var_i )));
        float var_3 = (v * ( 1.0 - s * ( 1.0 - ( var_h - var_i ) ) ));
       
        if      ( var_i == 0 ) { r1 = v     ; g1 = var_3 ; b1 = var_1; }
        else if ( var_i == 1 ) { r1 = var_2 ; g1 = v     ; b1 = var_1; }
        else if ( var_i == 2 ) { r1 = var_1 ; g1 = v     ; b1 = var_3; }
        else if ( var_i == 3 ) { r1 = var_1 ; g1 = var_2 ; b1 = v;     }
        else if ( var_i == 4 ) { r1 = var_3 ; g1 = var_1 ; b1 = v;     }
        else                   { r1 = v     ; g1 = var_1 ; b1 = var_2; }

        *r = (unsigned char)(r1 * 255);
        *g = (unsigned char)(g1 * 255);
        *b = (unsigned char)(b1 * 255);
    }
}

/**
 * Parameters of the dump_profiling_init function
 */
typedef struct profiling_init_info {
    string_arena_t *sa;
    int idx;
    int maxidx;
} profiling_init_info_t;

/**
 * dump_profiling_init:
 *  Takes the pointer to the name of a function, an index in
 *  a pointer to a dump_profiling_init, and prints 
 *    dague_profiling_add_dictionary_keyword( "elem", attribute[idx], &elem_key_start, &elem_key_end);
 *  into profiling_init_info.sa
 */
static char *dump_profiling_init(void **elem, void *arg)
{
    unsigned char R, G, B;
    char *fname = *(char**)elem;
    profiling_init_info_t *info = (profiling_init_info_t*)arg;
    
    string_arena_init(info->sa);

    get_unique_rgb_color((float)info->idx / (float)info->maxidx, &R, &G, &B);
    info->idx++;

    string_arena_add_string(info->sa,
                            "dague_profiling_add_dictionary_keyword(\"%s\", \"fill:%02X%02X%02X\",\n"
                            "                                         &res->%s_start_key,\n"
                            "                                         &res->%s_end_key);",
                            fname, R, G, B, fname, fname);

    return string_arena_get_string(info->sa);
}

static char *dump_data_repository_constructor(void **elem, void *arg)
{
    string_arena_t *sa = (string_arena_t*)arg;
    jdf_function_entry_t *f = (jdf_function_entry_t *)elem;
    int nbdata;

    string_arena_init(sa);

    JDF_COUNT_LIST_ENTRIES(f->dataflow, jdf_dataflow_list_t, next, nbdata);

    string_arena_add_string(sa, 
                            "  %s_nblocal_tasks = %s_%s_enumerate_local_tasks(res);\n"
                            "  %s_%s_allocate_dependencies(res);\n"
                            "  res->%s_repository = data_repo_create_nothreadsafe(\n"
                            "         ((unsigned int)(%s_nblocal_tasks * 1.5)) > MAX_DATAREPO_HASH ?\n"
                            "         MAX_DATAREPO_HASH :\n"
                            "         ((unsigned int)(%s_nblocal_tasks * 1.5)), %d);",
                            f->fname, jdf_basename, f->fname,
                            jdf_basename, f->fname,
                            f->fname, f->fname, f->fname, nbdata);

    return string_arena_get_string(sa);
}

/** Utils: observers for the jdf **/

static int jdf_symbol_is_global(const jdf_global_entry_t *globals, const char *name)
{
    const jdf_global_entry_t *g;
    for(g = globals; NULL != g; g = g->next)
        if( !strcmp(g->name, name) )
            return 1;
    return 0;
}

static int jdf_symbol_is_standalone(const char *name, const jdf_global_entry_t *globals, const jdf_expr_t *e)
{
    if( JDF_OP_IS_CST(e->op) )
        return 1;
    else if ( JDF_OP_IS_VAR(e->op) )
        return ((!strcmp(e->jdf_var, name)) ||
                jdf_symbol_is_global(globals, e->jdf_var));
    else if ( JDF_OP_IS_UNARY(e->op) )
        return jdf_symbol_is_standalone(name, globals, e->jdf_ua);
    else if ( JDF_OP_IS_TERNARY(e->op) )
        return jdf_symbol_is_standalone(name, globals, e->jdf_tat) &&
            jdf_symbol_is_standalone(name, globals, e->jdf_ta1) &&
            jdf_symbol_is_standalone(name, globals, e->jdf_ta2);
    else 
        return jdf_symbol_is_standalone(name, globals, e->jdf_ba1) &&
            jdf_symbol_is_standalone(name, globals, e->jdf_ba2);
}

static int jdf_expr_depends_on_symbol(const char *name, const jdf_expr_t *e)
{
    if( JDF_OP_IS_CST(e->op) )
        return 0;
    else if ( JDF_OP_IS_VAR(e->op) )
        return !strcmp(e->jdf_var, name);
    else if ( JDF_OP_IS_UNARY(e->op) )
        return jdf_expr_depends_on_symbol(name, e->jdf_ua);
    else if ( JDF_OP_IS_TERNARY(e->op) )
        return jdf_expr_depends_on_symbol(name, e->jdf_tat) ||
            jdf_expr_depends_on_symbol(name, e->jdf_ta1) ||
            jdf_expr_depends_on_symbol(name, e->jdf_ta2);
    else 
        return jdf_expr_depends_on_symbol(name, e->jdf_ba1) ||
            jdf_expr_depends_on_symbol(name, e->jdf_ba2);
}

static int jdf_dataflow_type(const jdf_dataflow_t *flow)
{
    jdf_dep_list_t *dl;
    int type = 0;
    for(dl = flow->deps; dl != NULL; dl = dl->next) {
        type |= dl->dep->type;
    }
    return type;
}

static int jdf_data_output_index(const jdf_t *jdf, const char *fname, const char *varname)
{
    int i;
    jdf_function_entry_t *f;
    jdf_dataflow_list_t *fl;
    
    i = 0;
    for(f = jdf->functions; f != NULL; f = f->next) {
        if( !strcmp(f->fname, fname) ) {
            for( fl = f->dataflow; fl != NULL; fl = fl->next) {
                if( jdf_dataflow_type(fl->flow) & JDF_DEP_TYPE_OUT ) {
                    if( !strcmp(fl->flow->varname, varname) ) {
                        return i;
                    }
                    i++;
                }
            }
            return -1;
        }
    }
    return -2;
}

static void jdf_coutput_prettycomment(char marker, const char *format, ...)
{
    int ls, rs, i;
    va_list ap, ap2;
    int length;
    char *v;
    int vs;

    vs = 80;
    v = (char *)malloc(vs);

    va_start(ap, format);
    /* va_list might have pointer to internal state and using
       it twice is a bad idea.  So make a copy for the second
       use.  Copy order taken from Autoconf docs. */
#if defined(HAVE_VA_COPY)
    va_copy(ap2, ap);
#elif defined(HAVE_UNDERSCORE_VA_COPY)
    __va_copy(ap2, ap);
#else
    memcpy (&ap2, &ap, sizeof(va_list));
#endif

    length = vsnprintf(v, vs, format, ap);
    if( length >= vs ) {
        /* realloc */
        vs = length + 1;
        v = (char*)realloc( v, vs );
        length = vsnprintf(v, vs, format, ap2);
    }

#if defined(HAVE_VA_COPY) || defined(HAVE_UNDERSCORE_VA_COPY)
    va_end(ap2);
#endif  /* defined(HAVE_VA_COPY) || defined(HAVE_UNDERSCORE_VA_COPY) */
    va_end(ap);

    /* Pretty printing */
    ls = strlen(v)/2;
    rs = strlen(v)-ls;
    if( ls > 40 ) ls = 40;
    if( rs > 40 ) rs = 40;
    coutput("/*");
    for(i = 0; i < 80; i++)
        coutput("%c", marker);
    coutput("*\n");
    coutput(" *%*s%s%*s*\n", 40-ls, " ", v, 40-rs, " ");
    coutput(" *");
    for(i = 0; i < 80; i++)
        coutput("%c", marker);
    coutput("*/\n\n");            
}

/** Structure Generators **/

static void jdf_generate_header_file(const jdf_t* jdf)
{
    string_arena_t *sa1, *sa2;

    sa1 = string_arena_new(64);
    sa2 = string_arena_new(64);

    houtput("#ifndef _%s_h_\n"
            "#define _%s_h_\n",
            jdf_basename, jdf_basename);
    houtput("#include <dague.h>\n\n");

    houtput("typedef struct dague_%s_object {\n", jdf_basename);
    houtput("  dague_object_t super;\n");
    houtput("  /* The list of globals */\n"
            "%s",
            UTIL_DUMP_LIST_FIELD( sa1, jdf->globals, next, name,
                                  dump_string, NULL, "", "  int ", ";\n", ";\n"));
    houtput("  /* The list of data */\n"
            "%s", 
            UTIL_DUMP_LIST_FIELD( sa1, jdf->data, next, dname,
                                  dump_string, NULL, "", "  dague_ddesc_t *", ";\n", ";\n"));
    houtput("} dague_%s_object_t;\n", jdf_basename);
    
    houtput("dague_%s_object_t *dague_%s_new(%s, %s);\n", jdf_basename, jdf_basename,
            UTIL_DUMP_LIST_FIELD( sa1, jdf->data, next, dname,
                                  dump_string, NULL, "", "dague_ddesc_t *", ", ", ""),
            UTIL_DUMP_LIST_FIELD( sa2, jdf->globals, next, name,
                                  dump_string, NULL, "",  "int ", ", ", ""));
    string_arena_free(sa1);
    string_arena_free(sa2);
    houtput("#endif /* _%s_h_ */ \n",
            jdf_basename);
}

static void jdf_generate_structure(const jdf_t *jdf)
{
    int nbfunctions, nbdata;
    string_arena_t *sa1, *sa2;
    jdf_preamble_entry_t *p;

    JDF_COUNT_LIST_ENTRIES(jdf->functions, jdf_function_entry_t, next, nbfunctions);
    JDF_COUNT_LIST_ENTRIES(jdf->data, jdf_data_entry_t, next, nbdata);

    sa1 = string_arena_new(64);
    sa2 = string_arena_new(64);

    for(p = jdf->preambles; p != NULL; p = p->next) {
        coutput("#line %d \"%s.jdf\"\n"
                "%s\n"
                "#line %d \"%s\"\n",
                p->lineno, jdf_basename,
                p->preamble,
                3 + nblines(p->preamble), jdf_cfilename);
    }

    coutput("#include <dague.h>\n"
            "#include <assignment.h>\n"
            "#include <remote_dep.h>\n"
            "#include \"%s.h\"\n\n"
            "#define DAGUE_%s_NB_FUNCTIONS %d\n"
            "#define DAGUE_%s_NB_DATA %d\n"
            "#if defined(DISTRIBUTED)\n"
            "#define IFDISTRIBUTED(t) t\n"
            "#else\n"
            "#define IFDISTRIBUTED(t) NULL\n"
            "#endif\n"
            "#if defined(DAGUE_PROFILING)\n"
            "#define TAKE_TIME(context, key, id) dague_profiling_trace(context->eu_profile, __dague_object->key, id)\n"
            "#else\n"
            "#define TAKE_TIME(context, key, id)\n"
            "#endif\n", 
            jdf_basename, 
            jdf_basename, nbfunctions, 
            jdf_basename, nbdata);
    coutput("typedef struct __dague_%s_internal_object {\n", jdf_basename);
    coutput(" dague_%s_object_t super;\n",
            jdf_basename);
    coutput("  /* The list of data repositories */\n"
            "%s",
            UTIL_DUMP_LIST_FIELD( sa1, jdf->functions, next, fname,
                                  dump_string, NULL, "", "  data_repo_t *", "_repository;\n", "_repository;\n"));
    coutput("  /* If profiling is enabled, the keys for profiling */\n"
            "#  if defined(DAGUE_PROFILING)\n"
            "%s", 
            UTIL_DUMP_LIST_FIELD( sa1, jdf->functions, next, fname,
                                  dump_string, NULL, "", "  int ", "_start_key;\n", "_start_key;\n"));
    coutput("%s", 
            UTIL_DUMP_LIST_FIELD( sa1, jdf->functions, next, fname,
                                  dump_string, NULL, "", "  int ", "_end_key;\n", "_end_key;\n"));
    coutput("#  endif /* defined(DAGUE_PROFILING) */\n");
    coutput("} __dague_%s_internal_object_t;\n"
            "\n", jdf_basename);

    coutput("/* Globals */\n%s\n",
            UTIL_DUMP_LIST_FIELD(sa1, jdf->globals, next, name,
                                 dump_globals, sa2, "", "#define ", "\n", "\n"));

    coutput("/* Data Access Macros */\n%s\n",
            UTIL_DUMP_LIST(sa1, jdf->data, next,
                           dump_data, sa2, "", "#define ", "\n", "\n"));

    coutput("/* Functions Predicates */\n%s\n",
            UTIL_DUMP_LIST(sa1, jdf->functions, next,
                           dump_predicate, sa2, "", "#define ", "\n", "\n"));

    coutput("/* Data Repositories */\n%s\n",
            UTIL_DUMP_LIST(sa1, jdf->functions, next,
                           dump_repo, sa2, "", "#define ", "\n", "\n"));

    coutput("/* Dependency Tracking Allocation Macro */\n"
            "#define ALLOCATE_DEP_TRACKING(DEPS, vMIN, vMAX, vNAME, vSYMBOL, PREVDEP, FLAG)               \\\n"
            "do {                                                                                         \\\n"
            "  int _vmin = (vMIN);                                                                        \\\n"
            "  int _vmax = (vMAX);                                                                        \\\n"
            "  (DEPS) = (dague_dependencies_t*)calloc(1, sizeof(dague_dependencies_t) +                   \\\n"
            "                   (_vmax - _vmin - 1) * sizeof(dague_dependencies_union_t));                \\\n"
            "  /*DEBUG((\"Allocate %%d spaces for loop %%s (min %%d max %%d) 0x%%p last_dep 0x%%p\\n\", */         \\\n"
            "  /*       (_vmax - _vmin + 1), (vNAME), _vmin, _vmax, (void*)(DEPS), (void*)(PREVDEP))); */ \\\n"
            "  (DEPS)->flags = DAGUE_DEPENDENCIES_FLAG_ALLOCATED | (FLAG);                                \\\n"
            "  DAGUE_STAT_INCREASE(mem_bitarray,  sizeof(dague_dependencies_t) + STAT_MALLOC_OVERHEAD +   \\\n"
            "                   (_vmax - _vmin - 1) * sizeof(dague_dependencies_union_t));                \\\n"
            "  (DEPS)->symbol = (vSYMBOL);                                                                \\\n"
            "  (DEPS)->min = _vmin;                                                                       \\\n"
            "  (DEPS)->max = _vmax;                                                                       \\\n"
            "  (DEPS)->prev = (PREVDEP); /* chain them backward */                                        \\\n"
            "} while (0)                                                                                  \n\n");

    string_arena_free(sa1);
    string_arena_free(sa2);
}

static void jdf_generate_expression( const jdf_t *jdf, const jdf_def_list_t *context,
                                     const jdf_expr_t *e, const char *name)
{
    string_arena_t *sa = string_arena_new(64);
    string_arena_t *sa2 = string_arena_new(64);
    string_arena_t *sa3 = string_arena_new(64);
    expr_info_t info;
    assignment_info_t ai;

    if( e->op == JDF_RANGE ) {
        char *subf = (char*)malloc(strlen(name) + 64);
        sprintf(subf, "rangemin_of_%s", name);
        jdf_generate_expression(jdf, context, e->jdf_ba1, subf);
        sprintf(subf, "rangemax_of_%s", name);
        jdf_generate_expression(jdf, context, e->jdf_ba2, subf);

        coutput("static const expr_t %s = {\n"
                "  .op = EXPR_OP_BINARY_RANGE,\n"
                "  .flags = 0x0,\n"
                "  .u_expr.binary.op1 = &rangemin_of_%s,\n"
                "  .u_expr.binary.op2 = &rangemax_of_%s\n"
                "};\n",
                name, name, name);
    } else {
        info.sa = sa;
        info.prefix = "";
        ai.sa = sa3;
        ai.idx = 0;
        ai.holder = "assignments";
        ai.expr = e;
        coutput("static inline int %s_fct(const dague_object_t *__dague_object_parent, const assignment_t *assignments)\n"
                "{\n"
                "  const __dague_%s_internal_object_t *__dague_object = (const __dague_%s_internal_object_t*)__dague_object_parent;\n"
                "%s\n"
                "  (void)__dague_object;\n"
                "  return %s;\n"
                "}\n", name, jdf_basename, jdf_basename,
                UTIL_DUMP_LIST_FIELD(sa2, context, next, name, 
                                     dump_assignments, &ai, "", "  int ", "", ""),
                dump_expr((void**)&e, &info));

        coutput("static const expr_t %s = {\n"
                "  .op = EXPR_OP_INLINE,\n"
                "  .flags = 0x0,\n"
                "  .inline_func = %s_fct\n"
                "};\n", name, name);
    }

    string_arena_free(sa);
    string_arena_free(sa2);
    string_arena_free(sa3);
}

static void jdf_generate_predicate_expr( const jdf_t *jdf, const jdf_def_list_t *context,
                                         const char *fname, const char *name)
{
    string_arena_t *sa = string_arena_new(64);
    string_arena_t *sa2 = string_arena_new(64);
    string_arena_t *sa3 = string_arena_new(64);
    string_arena_t *sa4 = string_arena_new(64);
    string_arena_t *sa5 = string_arena_new(64);
    expr_info_t info;
    assignment_info_t ai;

    info.sa = sa;
    info.prefix = "";
    ai.sa = sa3;
    ai.idx = 0;
    ai.holder = "assignments";
    ai.expr = NULL;
    coutput("static inline int %s_fct(const dague_object_t *__dague_object_parent, const assignment_t *assignments)\n"
            "{\n"
            "  const __dague_%s_internal_object_t *__dague_object = (const __dague_%s_internal_object_t*)__dague_object_parent;\n"
            "%s\n"
            "  /* Silent Warnings: should look into predicate to know what variables are usefull */\n"
            "%s\n"
            "  /* Compute Predicate */\n"
            "  return %s_pred%s;\n"
            "}\n", name, jdf_basename, jdf_basename,
            UTIL_DUMP_LIST_FIELD(sa2, context, next, name, 
                                 dump_assignments, &ai, "", "  int ", "", ""),
            UTIL_DUMP_LIST_FIELD(sa5, context, next, name,
                                 dump_string, NULL, "", "  (void)", ";\n", ";"),
            fname, 
            UTIL_DUMP_LIST_FIELD(sa4, context, next, name,
                                 dump_string, NULL, "(", "", ", ", ")"));
    string_arena_free(sa);
    string_arena_free(sa2);
    string_arena_free(sa3);
    string_arena_free(sa4);
    string_arena_free(sa5);

    coutput("static const expr_t %s = {\n"
            "  .op = EXPR_OP_INLINE,\n"
            "  .flags = 0x0,\n"
            "  .inline_func = %s_fct\n"
            "};\n", name, name);
}

static void jdf_generate_symbols( const jdf_t *jdf, const jdf_def_list_t *def, const char *prefix )
{
    const jdf_def_list_t *d;
    char *exprname;
    string_arena_t *sa = string_arena_new(64);

    for(d = def; d != NULL; d = d->next) {
        exprname = (char*)malloc(strlen(d->name) + strlen(prefix) + 16);
        string_arena_init(sa);
        string_arena_add_string(sa, "static const symbol_t %s%s = {", prefix, d->name);
        if( d->expr->op == JDF_RANGE ) {
            sprintf(exprname, "minexpr_of_%s%s", prefix, d->name);
            string_arena_add_string(sa, ".min = &%s, ", exprname);
            jdf_generate_expression(jdf, def, d->expr->jdf_ba1, exprname);

            sprintf(exprname, "maxexpr_of_%s%s", prefix, d->name);
            string_arena_add_string(sa, ".max = &%s, ", exprname);
            jdf_generate_expression(jdf, def, d->expr->jdf_ba2, exprname);
        } else {
            sprintf(exprname, "expr_of_%s%s", prefix, d->name);
            string_arena_add_string(sa, ".min = &%s, ", exprname);
            string_arena_add_string(sa, ".max = &%s, ", exprname);
            jdf_generate_expression(jdf, def, d->expr, exprname);
        }

        if( jdf_symbol_is_global(jdf->globals, d->name) ) {
            string_arena_add_string(sa, " .flags = DAGUE_SYMBOL_IS_GLOBAL");
        } else if ( jdf_symbol_is_standalone(d->name, jdf->globals, d->expr) ) {
            string_arena_add_string(sa, " .flags = DAGUE_SYMBOL_IS_STANDALONE");
        } else {
            string_arena_add_string(sa, " .flags = 0x0");
        }
        string_arena_add_string(sa, "};");
        coutput("%s\n\n", string_arena_get_string(sa));
        free(exprname);
    }

    string_arena_free(sa);
}

static void jdf_generate_dependency( const jdf_t *jdf, const char *datatype, 
                                     const jdf_call_t *call, const char *depname,
                                     const char *condname, const jdf_def_list_t *context )
{
    string_arena_t *sa = string_arena_new(64);
    jdf_expr_list_t *le;
    char *exprname;
    int i;
    char pre[8];

    string_arena_add_string(sa, 
                            "static const dep_t %s = {\n"
                            "  .cond = %s,\n"
                            "  .dague = &%s_%s,\n",
                            depname, 
                            condname,
                            jdf_basename, call->func_or_mem);

    if( call->var != NULL ) {
            string_arena_add_string(sa, 
                                    "  .param = &param_of_%s_%s_for_%s,\n",
                                    jdf_basename, call->func_or_mem, call->var);
    }
    string_arena_add_string(sa, 
                            "  .type  = IFDISTRIBUTED((void*)&%s), /**< Change this for C-code */\n"
                            "  .call_params = {\n",
                            datatype != NULL ? datatype : "DAGUE_DEFAULT_DATA_TYPE");

    exprname = (char *)malloc(strlen(depname) + 32);
    pre[0] = '\0';
    for( i = 1, le = call->parameters; le != NULL; i++, le = le->next ) {
        sprintf(exprname, "expr_of_p%d_for_%s", i, depname);
        string_arena_add_string(sa, "%s    &%s", pre, exprname);
        jdf_generate_expression(jdf, context, le->expr, exprname);
        sprintf(pre, ",\n");
    }
    free(exprname);

    string_arena_add_string(sa, 
                            "\n"
                            "  }\n"
                            "};\n");
    coutput("%s", string_arena_get_string(sa));

    string_arena_free(sa);
}

static void jdf_generate_dataflow( const jdf_t *jdf, const jdf_def_list_t *context,
                                   jdf_dataflow_t *flow, const char *prefix, unsigned char mask )
{
    string_arena_t *sa = string_arena_new(64);
    string_arena_t *sa_dep_in = string_arena_new(64);
    string_arena_t *sa_dep_out = string_arena_new(64);
    string_arena_t *psa;
    int alldeps_type;
    jdf_dep_list_t *dl;
    char *sym_type;
    char *access_type;
    int depid;
    char *depname;
    char *condname;
    char sep_in[3], sep_out[3];
    char *sep;

    string_arena_init(sa_dep_in);
    string_arena_init(sa_dep_out);
    
    depname = (char*)malloc(strlen(prefix) + strlen(flow->varname) + 128);
    condname = (char*)malloc(strlen(prefix) + strlen(flow->varname) + 128);
    sep_in[0] = '\0';
    sep_out[0] = '\0';

    for(depid = 1, dl = flow->deps; dl != NULL; depid++, dl = dl->next) {
        if( dl->dep->type == JDF_DEP_TYPE_IN ) {
            psa = sa_dep_in;
            sep = sep_in;
        }
        else if ( dl->dep->type == JDF_DEP_TYPE_OUT ) {
            psa = sa_dep_out;
            sep = sep_out;
        } else {
            jdf_fatal(dl->dep->lineno, "This dependency is neither a DEP_IN or a DEP_OUT?\n");
            exit(1);
        }

        if( dl->dep->guard->guard_type == JDF_GUARD_UNCONDITIONAL ) {
            sprintf(depname, "%s%s_dep%d_atline_%d", prefix, flow->varname, depid, dl->dep->lineno);
            sprintf(condname, "NULL");
            jdf_generate_dependency(jdf, dl->dep->datatype, dl->dep->guard->calltrue, depname, condname, context);
            string_arena_add_string(psa, "%s&%s", sep, depname);
            sprintf(sep, ", ");
        } else if( dl->dep->guard->guard_type == JDF_GUARD_BINARY ) {
            sprintf(depname, "%s%s_dep%d_atline_%d", prefix, flow->varname, depid, dl->dep->lineno);
            sprintf(condname, "expr_of_cond_for_%s", depname);
            jdf_generate_expression(jdf, context, dl->dep->guard->guard, condname);
            sprintf(condname, "&expr_of_cond_for_%s", depname);
            jdf_generate_dependency(jdf, dl->dep->datatype, dl->dep->guard->calltrue, depname, condname, context);
            string_arena_add_string(psa, "%s&%s", sep, depname);
            sprintf(sep, ", ");
        } else if( dl->dep->guard->guard_type == JDF_GUARD_TERNARY ) {
            jdf_expr_t not;

            sprintf(depname, "%s%s_dep%d_iftrue_atline_%d", prefix, flow->varname, depid, dl->dep->lineno);
            sprintf(condname, "expr_of_cond_for_%s", depname);
            jdf_generate_expression(jdf, context, dl->dep->guard->guard, condname);
            sprintf(condname, "&expr_of_cond_for_%s", depname);
            jdf_generate_dependency(jdf, dl->dep->datatype, dl->dep->guard->calltrue, depname, condname, context);
            string_arena_add_string(psa, "%s&%s", sep, depname);
            sprintf(sep, ", ");

            sprintf(depname, "%s%s_dep%d_iffalse_atline_%d", prefix, flow->varname, depid, dl->dep->lineno);
            sprintf(condname, "expr_of_cond_for_%s", depname);
            not.op = JDF_NOT;
            not.jdf_ua = dl->dep->guard->guard;
            jdf_generate_expression(jdf, context, &not, condname);
            sprintf(condname, "&expr_of_cond_for_%s", depname);
            jdf_generate_dependency(jdf, dl->dep->datatype, dl->dep->guard->callfalse, depname, condname, context);
            string_arena_add_string(psa, "%s&%s", sep, depname);
        }
    }
    free(depname);
    free(condname);

    alldeps_type = jdf_dataflow_type(flow);
    sym_type = ( (alldeps_type == JDF_DEP_TYPE_IN) ? "SYM_IN" :
                 ((alldeps_type == JDF_DEP_TYPE_OUT) ? "SYM_OUT" : "SYM_INOUT") );

    access_type = ( (flow->access_type == JDF_VAR_TYPE_READ) ? "ACCESS_READ" :
                    ((flow->access_type == JDF_VAR_TYPE_WRITE) ? "ACCESS_WRITE" : "ACCESS_RW") ); 

    string_arena_add_string(sa, 
                            "static const param_t %s%s = {\n"
                            "  .name = \"%s\",\n"
                            "  .sym_type = %s,\n"
                            "  .access_type = %s,\n"
                            "  .param_mask = 0x%x,\n"
                            "  .dep_in  = { %s },\n"
                            "  .dep_out = { %s }\n"
                            "};\n\n", 
                            prefix, flow->varname, 
                            flow->varname, 
                            sym_type, 
                            access_type,
                            mask,
                            string_arena_get_string(sa_dep_in),
                            string_arena_get_string(sa_dep_out));
    string_arena_free(sa_dep_in);
    string_arena_free(sa_dep_out);

    coutput("%s", string_arena_get_string(sa));
    string_arena_free(sa);
}

static void jdf_generate_enumerate_locals(const jdf_t *jdf, const jdf_function_entry_t *f, const char *fname)
{
    string_arena_t *sa1, *sa2;
    jdf_def_list_t *dl;
    int nesting;
    expr_info_t info1, info2;

    sa1 = string_arena_new(64);
    sa2 = string_arena_new(64);

    coutput("static int %s(const __dague_cholesky_internal_object_t *__dague_object)\n"
            "{\n"
            "%s"
            "  int nb = 0;\n",
            fname,
            UTIL_DUMP_LIST_FIELD(sa1, f->definitions, next, name, dump_string, NULL,
                                 "", "  int ", ";\n", ";\n"));

    string_arena_init(sa1);

    info1.sa = sa1;
    info1.prefix = "";

    info2.sa = sa2;
    info2.prefix = "";

    nesting = 0;
    for(dl = f->definitions; dl != NULL; dl = dl->next) {
        if(dl->expr->op == JDF_RANGE) {
            coutput("%*s  for(%s = %s; %s <= %s; %s++) {\n",
                    nesting, "  ", 
                    dl->name, dump_expr((void**)&dl->expr->jdf_ba1, &info1),
                    dl->name, dump_expr((void**)&dl->expr->jdf_ba2, &info2),
                    dl->name);
            nesting++;
        } else {
            coutput("%*s  %s = %s;\n", nesting, "  ", dl->name, dump_expr((void**)&dl->expr, &info1));
        }
    }

    string_arena_init(sa1);
    coutput("%*s    if( %s_pred(%s) ) nb++;\n",
            nesting, "  ", f->fname, UTIL_DUMP_LIST_FIELD(sa1, f->parameters, next, name,
                                                          dump_string, NULL, 
                                                          "", "", ", ", ""));

    string_arena_free(sa1);    
    string_arena_free(sa2);

    for(; nesting > 0; nesting--) {
        coutput("%*s  }\n", nesting, "  ");
    }

    coutput("  return nb;\n"
            "}\n"
            "\n");
}

static void jdf_generate_allocate_dependencies(const jdf_t *jdf, const jdf_function_entry_t *f, const char *fname)
{
    string_arena_t *sa1, *sa2;
    jdf_def_list_t *dl, *pdl;
    int nesting;
    const jdf_function_entry_t *pf;
    expr_info_t info1, info2;

    sa1 = string_arena_new(64);
    sa2 = string_arena_new(64);

    coutput("static void %s(const __dague_cholesky_internal_object_t *__dague_object)\n"
            "{\n"
            "  dague_dependencies_t *dep;\n"
            "  int __foundone;\n"
            "%s",
            fname,
            UTIL_DUMP_LIST_FIELD(sa1, f->definitions, next, name, dump_string, NULL,
                                 "", "  int ", ";\n", ";\n"));
    coutput("%s"
            "%s",
            UTIL_DUMP_LIST_FIELD(sa1, f->definitions, next, name, dump_string, NULL,
                                 "", "  uint32_t ", "_min = 0xffffffff;\n", "_min = 0xffffffff;\n"),
            UTIL_DUMP_LIST_FIELD(sa2, f->definitions, next, name, dump_string, NULL,
                                 "", "  uint32_t ", "_max = 0;\n", "_max = 0;\n"));
    coutput("%s"
            "%s",
            UTIL_DUMP_LIST_FIELD(sa1, f->definitions, next, name, dump_string, NULL,
                                 "", "  int ", "_start;\n", "_start;\n"),
            UTIL_DUMP_LIST_FIELD(sa2, f->definitions, next, name, dump_string, NULL,
                                 "", "  int ", "_end;\n", "_end;\n"));

    string_arena_init(sa1);
    string_arena_init(sa2);

    info1.sa = sa1;
    info1.prefix = "";

    info2.sa = sa2;
    info2.prefix = "";

    coutput("  /* First, find the min and max value for each of the dimensions */\n");

    nesting = 0;
    for(dl = f->definitions; dl != NULL; dl = dl->next) {
        if(dl->expr->op == JDF_RANGE) {
            coutput("%*s  %s_start = %s;\n", 
                    nesting, "  ", dl->name, dump_expr((void**)&dl->expr->jdf_ba1, &info1));
            coutput("%*s  %s_end = %s;\n", 
                    nesting, "  ", dl->name, dump_expr((void**)&dl->expr->jdf_ba2, &info2));
            coutput("%*s  for(%s = %s_start; %s <= %s_end; %s++) {\n",
                    nesting, "  ", dl->name, dl->name, dl->name, dl->name, dl->name);
            nesting++;
        } else {
            coutput("%*s  %s = %s_start = %s_end = %s;\n", 
                    nesting, "  ", dl->name, dl->name, 
                    dl->name, dl->name, dump_expr((void**)&dl->expr, &info1));
        }
    }

    string_arena_init(sa1);
    coutput("%*s    if( !%s_pred(%s) ) continue;\n",
            nesting, "  ", f->fname, UTIL_DUMP_LIST_FIELD(sa1, f->parameters, next, name,
                                                          dump_string, NULL, 
                                                          "", "", ", ", ""));
    for(dl = f->definitions; dl != NULL; dl = dl->next) {
        coutput("%*s    %s_max = ( (%s < %s_max) ? %s_max : %s );\n"
                "%*s    %s_min = ( (%s > %s_min) ? %s_min : %s );\n",
                nesting, "  ", dl->name, dl->name, dl->name, dl->name, dl->name,
                nesting, "  ", dl->name, dl->name, dl->name, dl->name, dl->name);
    }

    for(; nesting > 0; nesting--) {
        coutput("%*s  }\n", nesting, "  ");
    }

    coutput("\n"
            "  /**\n"
            "   * Now, for each of the dimensions, re-iterate on the space,\n"
            "   * and if at least one value is defined, allocate arrays to point\n"
            "   * to it. Array dimensions are defined by the (rough) observation above\n"
            "   **/\n");

    if( f->definitions->next == NULL ) {
        coutput("  ALLOCATE_DEP_TRACKING(dep, %s_min, %s_max, \"%s\", &symb_%s_%s_%s, NULL, DAGUE_DEPENDENCIES_FLAG_FINAL);\n",
                f->definitions->name, f->definitions->name, f->definitions->name,
                jdf_basename, f->fname, f->definitions->name);
    } else {
        coutput("  dep = NULL;\n");

        nesting = 0;
        for(dl = f->definitions; dl != NULL; dl = dl->next ) {
            if( dl->next == NULL ) {
                coutput("%*s  __foundone = 0;\n", nesting, "  ");
            }
            if(dl->expr->op == JDF_RANGE) {
                coutput("%*s  %s_start = %s;\n", 
                        nesting, "  ", dl->name, dump_expr((void**)&dl->expr->jdf_ba1, &info1));
                coutput("%*s  %s_end = %s;\n", 
                        nesting, "  ", dl->name, dump_expr((void**)&dl->expr->jdf_ba2, &info2));
                coutput("%*s  for(%s = (%s_min < %s_start ? %s_start : %s_min); %s <= (%s_end < %s_max ? %s_end : %s_max); %s++) {\n",
                        nesting, "  ", dl->name, dl->name, dl->name, dl->name, dl->name, dl->name, dl->name, dl->name, dl->name, dl->name, dl->name);
                nesting++;
            } else {
                coutput("%*s  %s = %s_start = %s_end = %s;\n", 
                        nesting, "  ", dl->name, dl->name, 
                        dl->name, dl->name, dump_expr((void**)&dl->expr, &info1));
            }
        }

        coutput("%*s  if( %s_pred(%s) ) {\n"
                "%*s    __foundone = 1;\n"
                "%*s    break;\n"
                "%*s  }\n",
                nesting, "  ", f->fname, UTIL_DUMP_LIST_FIELD(sa2, f->parameters, next, name,
                                                              dump_string, NULL, 
                                                              "", "", ", ", ""),
                nesting, "  ",
                nesting, "  ",
                nesting, "  ");
        nesting--;
        coutput("%*s  }\n"
                "%*s  /* Did we find one? If not, skip this allocation, otherwise allocate */\n"
                "%*s  if( 0 == __foundone ) continue;\n",
                nesting, "  ",
                nesting, "  ",
                nesting, "  ");

        string_arena_init(sa1);
        string_arena_add_string(sa1, "dep");
        for(dl = f->definitions; dl != NULL; dl = dl->next ) {
            coutput("%*s    if( %s == NULL ) {\n"
                    "%*s      ALLOCATE_DEP_TRACKING(%s, %s_min, %s_max, \"%s\", &symb_%s_%s_%s, %s, %s);\n"
                    "%*s    }\n",
                    nesting, "  ", string_arena_get_string(sa1),
                    nesting, "  ", string_arena_get_string(sa1), dl->name, dl->name, dl->name, 
                                   jdf_basename, f->fname, dl->name,
                                   dl == f->definitions ? "NULL" : string_arena_get_string(sa2),
                                   dl->next == NULL ? "DAGUE_DEPENDENCIES_FLAG_FINAL" : "DAGUE_DEPENDENCIES_FLAG_NEXT",
                    nesting, "  ");
            string_arena_init(sa2);
            string_arena_add_string(sa2, "%s", string_arena_get_string(sa1));
            string_arena_add_string(sa1, "->u.next[%s-%s_min]", dl->name, dl->name);
        }
        
        for(; nesting > 0; nesting--) {
            coutput("%*s  }\n", nesting, "  ");
        }
    }

    string_arena_free(sa1);    
    string_arena_free(sa2);

    for(nesting = 0, pf = jdf->functions;
        strcmp( pf->fname, f->fname);
        nesting++, pf = pf->next) /* nothing */;

    coutput("  __dague_object->super.super.dependencies_array[%d] = dep;\n"
            "}\n"
            "\n",
            nesting);
}

static void jdf_generate_one_function( const jdf_t *jdf, const jdf_function_entry_t *f, int dep_index )
{
    string_arena_t *sa, *sa2;
    int nbparameters;
    int nbdataflow;
    int i;
    jdf_dataflow_list_t *fl;
    char *prefix;

    sa = string_arena_new(64);
    sa2 = string_arena_new(64);

    JDF_COUNT_LIST_ENTRIES(f->parameters, jdf_name_list_t, next, nbparameters);
    JDF_COUNT_LIST_ENTRIES(f->dataflow, jdf_dataflow_list_t, next, nbdataflow);
    
    jdf_coutput_prettycomment('*', "%s", f->fname);
    
    string_arena_add_string(sa, 
                            "static const dague_t %s_%s = {\n"
                            "  .name = \"%s\",\n"
                            "  .deps = %d,\n"
                            "  .flags = %s,\n"
                            "  .dependencies_mask = 0x0,\n"
                            "  .nb_locals = %d,\n"
                            "  .nb_params = %d,\n",
                            jdf_basename, f->fname,
                            f->fname,
                            dep_index,
                            (f->flags & JDF_FUNCTION_FLAG_HIGH_PRIORITY) ? "DAGUE_HIGH_PRIORITY_TASK" : "0x0",
                            nbparameters,
                            nbdataflow);

    prefix = (char*)malloc(strlen(f->fname) + strlen(jdf_basename) + 32);

    sprintf(prefix, "symb_%s_%s_", jdf_basename, f->fname);
    jdf_generate_symbols(jdf, f->definitions, prefix);
    sprintf(prefix, "&symb_%s_%s_", jdf_basename, f->fname);
    string_arena_add_string(sa, "  .params = { %s },\n", 
                            UTIL_DUMP_LIST_FIELD(sa2, f->parameters, next, name, dump_string, NULL,
                                                 "", prefix, ", ", ""));
    string_arena_add_string(sa, "  .locals = { %s },\n",
                            UTIL_DUMP_LIST_FIELD(sa2, f->definitions, next, name, dump_string, NULL,
                                                 "", prefix, ", ", ""));

    sprintf(prefix, "pred_of_%s_%s_as_expr", jdf_basename, f->fname);
    jdf_generate_predicate_expr(jdf, f->definitions, f->fname, prefix);
    string_arena_add_string(sa, "  .pred = &%s,\n", prefix);

    sprintf(prefix, "priority_of_%s_%s_as_expr", jdf_basename, f->fname);
    jdf_generate_expression(jdf, f->definitions, f->priority, prefix);
    string_arena_add_string(sa, "  .priority = &%s,\n", prefix);

    sprintf(prefix, "param_of_%s_%s_for_", jdf_basename, f->fname);
    for(i = 0, fl = f->dataflow; fl != NULL; fl = fl->next, i++) {
        jdf_generate_dataflow(jdf, f->definitions, fl->flow, prefix, 1<<i);
    }
    sprintf(prefix, "&param_of_%s_%s_for_", jdf_basename, f->fname);
    string_arena_add_string(sa, "  .in = { %s },\n",
                            UTIL_DUMP_LIST_FIELD(sa2, f->dataflow, next, flow, dump_dataflow, "IN",
                                                 "", prefix, ", ", ""));
    string_arena_add_string(sa, "  .out = { %s },\n",
                            UTIL_DUMP_LIST_FIELD(sa2, f->dataflow, next, flow, dump_dataflow, "OUT",
                                                 "", prefix, ", ", ""));

    sprintf(prefix, "iterate_successors_of_%s_%s", jdf_basename, f->fname);
    jdf_generate_code_iterate_successors(jdf, f, prefix);
    string_arena_add_string(sa, "  .iterate_successors = %s,\n", prefix);

    sprintf(prefix, "release_deps_of_%s_%s", jdf_basename, f->fname);
    jdf_generate_code_release_deps(jdf, f, prefix);
    string_arena_add_string(sa, "  .release_deps = %s,\n", prefix);

    sprintf(prefix, "hook_of_%s_%s", jdf_basename, f->fname);
    jdf_generate_code_hook(jdf, f, prefix);
    string_arena_add_string(sa, "  .hook = %s,\n", prefix);

    sprintf(prefix, "%s_%s_enumerate_local_tasks", jdf_basename, f->fname);
    jdf_generate_enumerate_locals(jdf, f, prefix);

    sprintf(prefix, "%s_%s_allocate_dependencies", jdf_basename, f->fname);
    jdf_generate_allocate_dependencies(jdf, f, prefix);

    free(prefix);

    string_arena_add_string(sa, "};\n");

    coutput("%s\n\n", string_arena_get_string(sa));

    string_arena_free(sa2);
    string_arena_free(sa);
}

static void jdf_generate_functions_statics( const jdf_t *jdf )
{
    jdf_function_entry_t *f;
    string_arena_t *sa;
    int i;

    sa = string_arena_new(64);
    string_arena_add_string(sa, "static const dague_t *%s_functions[] = {\n",
                            jdf_basename);
    for(i = 0, f = jdf->functions; NULL != f; f = f->next, i++) {
        jdf_generate_one_function(jdf, f, i);
        string_arena_add_string(sa, "  &%s_%s%s\n", 
                                jdf_basename, f->fname, f->next != NULL ? "," : "");
    }
    string_arena_add_string(sa, "};\n\n");
    coutput("%s", string_arena_get_string(sa));

    string_arena_free(sa);
}

static char *dump_pseudodague(void **elem, void *arg)
{
    string_arena_t *sa = (string_arena_t *)arg;
    char *name = *(char**)elem;
    string_arena_init(sa);
    string_arena_add_string(sa,
                            "static const dague_t %s_%s = {\n"
                            "  .name = \"%s\",\n"
                            "  .flags = 0x0,\n"
                            "  .dependencies_mask = 0x0,\n"
                            "  .nb_locals = 0,\n"
                            "  .nb_params = 0,\n"
                            "  .params = { NULL, },\n"
                            "  .locals = { NULL, },\n"
                            "  .pred = NULL,\n"
                            "  .in = { NULL, },\n"
                            "  .out = { NULL, },\n"
                            "  .priority = NULL,\n"
                            "  .deps = -1,\n"
                            "  .hook = NULL,\n"
                            "  .release_deps = NULL,\n"
                            "  .body = NULL,\n"
                            "#if defined(DAGUE_CACHE_AWARENESS)\n"
                            "  .cache_rank_function = NULL,\n"
                            "#endif /* defined(DAGUE_CACHE_AWARENESS) */\n"
                            "};\n",
                            jdf_basename, name, name);
    return string_arena_get_string(sa);
}

static void jdf_generate_predeclarations( const jdf_t *jdf )
{
    jdf_function_entry_t *f;
    int fli, depid;
    jdf_dataflow_list_t *fl;
    jdf_dep_list_t *dl;
    string_arena_t *sa = string_arena_new(64);
    string_arena_t *sa2 = string_arena_new(64);

    coutput("/** Predeclarations of the dague_t objects */\n");
    for(f = jdf->functions; f != NULL; f = f->next) {
        coutput("static const dague_t %s_%s;\n", jdf_basename, f->fname);
        coutput("static inline int priority_of_%s_%s_as_expr_fct(const dague_object_t *__dague_object_parent, const assignment_t *assignments);\n", 
                jdf_basename, f->fname);
    }
    coutput("/** Declarations of the pseudo-dague_t objects for data */\n"
            "%s\n",
            UTIL_DUMP_LIST_FIELD(sa2, jdf->data, next, dname,
                                 dump_pseudodague, sa, "", "", "", ""));
    string_arena_free(sa);
    string_arena_free(sa2);
    coutput("/** ¨Predeclarations of the parameters */\n");
    for(f = jdf->functions; f != NULL; f = f->next) {
        for(fl = f->dataflow; fl != NULL; fl = fl->next) {
            for(depid = 1, dl = fl->flow->deps; dl != NULL; depid++, dl = dl->next) {
                if( (dl->dep->guard->guard_type == JDF_GUARD_UNCONDITIONAL) ||
                    (dl->dep->guard->guard_type == JDF_GUARD_BINARY) ) {
                    if( dl->dep->guard->calltrue->var != NULL ) {
                        coutput("static const param_t param_of_%s_%s_for_%s;\n", 
                                jdf_basename, dl->dep->guard->calltrue->func_or_mem, dl->dep->guard->calltrue->var);
                    } 
                } else {
                    /* dl->dep->guard->guard_type == JDF_GUARD_TERNARY */
                    if( dl->dep->guard->calltrue->var != NULL ) {
                        coutput("static const param_t param_of_%s_%s_for_%s;\n", 
                                jdf_basename, dl->dep->guard->calltrue->func_or_mem, dl->dep->guard->calltrue->var);
                    } 
                    if( dl->dep->guard->callfalse->var != NULL ) {
                        coutput("static const param_t param_of_%s_%s_for_%s;\n", 
                                jdf_basename, dl->dep->guard->callfalse->func_or_mem, dl->dep->guard->callfalse->var);
                    }
                }
            }
        }
    }
}

static void jdf_generate_constructor( const jdf_t* jdf )
{
    string_arena_t *sa1,*sa2;
    profiling_init_info_t pi;
    sa1 = string_arena_new(64);
    sa2 = string_arena_new(64);

    coutput("%s\n",
            UTIL_DUMP_LIST_FIELD( sa1, jdf->globals, next, name,
                                  dump_string, NULL, "", "#undef ", "\n", "\n"));

    coutput("dague_%s_object_t *dague_%s_new(%s, %s)\n{\n", jdf_basename, jdf_basename,
            UTIL_DUMP_LIST_FIELD( sa1, jdf->data, next, dname,
                                  dump_string, NULL, "", "dague_ddesc_t *", ", ", ""),
            UTIL_DUMP_LIST_FIELD( sa2, jdf->globals, next, name,
                                  dump_string, NULL, "",  "int ", ", ", ""));

    coutput("  __dague_%s_internal_object_t *res = (__dague_%s_internal_object_t *)calloc(1, sizeof(__dague_%s_internal_object_t));\n",
            jdf_basename, jdf_basename, jdf_basename);


    string_arena_init(sa1);
    coutput("%s\n",
            UTIL_DUMP_LIST_FIELD( sa1, jdf->functions, next, fname,
                                  dump_string, NULL, "", "  int ", "_nblocal_tasks;\n", "_nblocal_tasks;\n") );

    coutput("  res->super.super.nb_functions    = DAGUE_%s_NB_FUNCTIONS;\n", jdf_basename);
    coutput("  res->super.super.functions_array = (const dague_t**)malloc(DAGUE_%s_NB_FUNCTIONS * sizeof(dague_t*));\n",
            jdf_basename);
    coutput("  res->super.super.dependencies_array = (dague_dependencies_t **)\n"
            "             calloc(DAGUE_%s_NB_FUNCTIONS, sizeof(dague_dependencies_t *));\n",
            jdf_basename);
    coutput("  memcpy(res->super.super.functions_array, %s_functions, DAGUE_%s_NB_FUNCTIONS * sizeof(dague_t*));\n",
            jdf_basename, jdf_basename);

    coutput("  /* Now the Parameter-dependent structures: */\n");

    coutput("%s", UTIL_DUMP_LIST_FIELD(sa1, jdf->data, next, dname,
                                       dump_resinit, sa2, "", "  ", "\n", "\n"));
    coutput("%s", UTIL_DUMP_LIST_FIELD(sa1, jdf->globals, next, name,
                                       dump_resinit, sa2, "", "  ", "\n", "\n"));

    pi.sa = sa2;
    pi.idx = 0;
    JDF_COUNT_LIST_ENTRIES(jdf->functions, jdf_function_entry_t, next, pi.maxidx);
    coutput("  /* If profiling is enabled, the keys for profiling */\n"
            "#  if defined(DAGUE_PROFILING)\n"
            "%s"
            "#  endif /* defined(DAGUE_PROFILING) */\n", 
            UTIL_DUMP_LIST_FIELD( sa1, jdf->functions, next, fname,
                                  dump_profiling_init, &pi, "", "  ", "\n", "\n"));

    coutput("  /* Create the data repositories for this object */\n"
            "%s",
            UTIL_DUMP_LIST( sa1, jdf->functions, next, dump_data_repository_constructor, sa2,
                            "", "", "\n", "\n"));

    string_arena_init(sa1);
    coutput("  res->super.super.nb_local_tasks = %s;\n",
            UTIL_DUMP_LIST_FIELD( sa1, jdf->functions, next, fname,
                                  dump_string, NULL, "", "", "_nblocal_tasks + ", "_nblocal_tasks") );

    string_arena_init(sa1);

    coutput("  return (dague_%s_object_t*)res;\n"
            "}\n\n", jdf_basename);

    string_arena_free(sa1);
    string_arena_free(sa2);
}

static void jdf_generate_hashfunction_for(const jdf_t *jdf, const jdf_function_entry_t *f)
{
    string_arena_t *sa = string_arena_new(64);
    jdf_def_list_t *dl;
    expr_info_t info;

    coutput("static inline int %s_hash(const __dague_%s_internal_object_t *__dague_object, %s)\n"
            "{\n"
            "  int __h = 0;\n",
            f->fname, jdf_basename, UTIL_DUMP_LIST_FIELD(sa, f->parameters, next, name,
                                                         dump_string, NULL, "", "int ", ", ", ""));

    info.prefix = "";
    info.sa = sa;

    for(dl = f->definitions; dl != NULL; dl = dl->next) {
        string_arena_init(sa);
        if( dl->expr->op == JDF_RANGE ) {
            coutput("  int %s_min = %s;\n", dl->name, dump_expr((void**)&dl->expr->jdf_ba1, &info));
            if( dl->next != NULL ) {
                coutput("  int %s_range = %s - %s_min + 1;\n", 
                        dl->name, dump_expr((void**)&dl->expr->jdf_ba2, &info), dl->name);
            }
        } else {
            coutput("  int %s_min = %s;\n", dl->name, dump_expr((void**)&dl->expr, &info));
            if( dl->next != NULL ) {
                coutput("  int %s_range = 1;\n", dl->name);
            }
        }
    }

    string_arena_init(sa);
    for(dl = f->definitions; dl != NULL; dl = dl->next) {
        coutput("  __h += (%s - %s_min)%s;\n",dl->name, dl->name, string_arena_get_string(sa));
        string_arena_add_string(sa, " * %s_range", dl->name);
    }

    coutput("  return __h;\n");
    coutput("}\n\n");
    string_arena_free(sa);
}

static void jdf_generate_hashfunctions(const jdf_t *jdf)
{
    jdf_function_entry_t *f;

    for(f = jdf->functions; f != NULL; f = f->next) {
        jdf_generate_hashfunction_for(jdf, f);
    }
}

/** Code Generators */

static void jdf_generate_code_call_initialization(const jdf_t *jdf, const jdf_call_t *call, 
                                                  int lineno, const char *fname, const jdf_dataflow_t *f,
                                                  const char *spaces)
{
    string_arena_t *sa, *sa2;
    expr_info_t info;
    int dataindex;

    sa = string_arena_new(64);
    sa2 = string_arena_new(64);

    info.sa = sa2;
    info.prefix = "";

    if( call->var != NULL ) {
        dataindex = jdf_data_output_index(jdf, call->func_or_mem,
                                          call->var);
        if( dataindex < 0 ) {
            if( dataindex == -1 ) {
                jdf_fatal(lineno, 
                          "During code generation: unable to find an output flow for variable %s in function %s,\n"
                          "which is requested by function %s to satisfy Input dependency at line %d\n",
                          call->var, call->func_or_mem,
                          fname, lineno);
                exit(1);
            } else {
                jdf_fatal(lineno, 
                          "During code generation: unable to find function %s,\n"
                          "which is requested by function %s to satisfy Input dependency at line %d\n",
                          call->func_or_mem,
                          fname, lineno);
                exit(1);
            }
        }
        coutput("%s  e%s = data_repo_lookup_entry( %s_repo, %s_hash( __dague_object, %s ));\n"
                "%s  g%s = e%s->data[%d];\n",
                spaces, f->varname, call->func_or_mem, call->func_or_mem, 
                UTIL_DUMP_LIST_FIELD(sa, call->parameters, next, expr,
                                     dump_expr, &info, "", "", ", ", ""),
                spaces, f->varname, f->varname, dataindex);
    } else {
        coutput("%s  g%s = gc_data_new(%s(%s), 0);\n",
                spaces, f->varname, call->func_or_mem,
                UTIL_DUMP_LIST_FIELD(sa, call->parameters, next, expr,
                                     dump_expr, &info, "", "", ", ", ""));
    }

    string_arena_free(sa);
    string_arena_free(sa2);
}

static void jdf_generate_code_flow_initialization(const jdf_t *jdf, const char *fname, const jdf_dataflow_t *f)
{
    jdf_dep_list_t *dl;
    expr_info_t info;
    string_arena_t *sa;
    int input_dumped = 0;

    sa = string_arena_new(64);
    info.sa = sa;
    info.prefix = "";

    for(dl = f->deps; dl != NULL; dl = dl->next) {
        if( dl->dep->type == JDF_DEP_TYPE_OUT )
            /** No initialization for output-only flows */
            continue;

        if( (input_dumped == 1) && (JDF_COMPILER_GLOBAL_ARGS.wmask & JDF_WARN_MASKED_GLOBALS) ) {
            jdf_warn(f->lineno, 
                     "The flow of data %s from function %s has multiple inputs.\n"
                     "If the cases are not mutually exclusive, expect unpredicted results / deadlocks.\n",
                     f->varname, fname);
        }
        input_dumped = 1;
        switch( dl->dep->guard->guard_type ) {
        case JDF_GUARD_UNCONDITIONAL:
            jdf_generate_code_call_initialization( jdf, dl->dep->guard->calltrue, f->lineno, fname, f, "" );
            break;
        case JDF_GUARD_BINARY:
            coutput("  if( %s ) {\n",
                    dump_expr((void**)&dl->dep->guard->guard, &info));
            jdf_generate_code_call_initialization( jdf, dl->dep->guard->calltrue, f->lineno, fname, f, "  " );
            coutput("  }\n");
            break;
        case JDF_GUARD_TERNARY:
            coutput("  if( %s ) {\n",
                    dump_expr((void**)&dl->dep->guard->guard, &info));
            jdf_generate_code_call_initialization( jdf, dl->dep->guard->calltrue, f->lineno, fname, f, "  " );
            coutput("  } else {\n");
            jdf_generate_code_call_initialization( jdf, dl->dep->guard->callfalse, f->lineno, fname, f, "  " );
            coutput("  }\n");
            break;
        }
        coutput("  %s = GC_DATA(g%s);\n", f->varname, f->varname);
    }

    string_arena_free(sa);
}

static void jdf_generate_code_call_final_write(const jdf_t *jdf, const jdf_call_t *call, const char *datatype,
                                               int lineno, const char *fname, const jdf_dataflow_t *f,
                                               const char *spaces)
{
    string_arena_t *sa, *sa2;
    expr_info_t info;
    int dataindex;

    sa = string_arena_new(64);
    sa2 = string_arena_new(64);

    info.sa = sa2;
    info.prefix = "";

    if( call->var == NULL ) {
        UTIL_DUMP_LIST_FIELD(sa, call->parameters, next, expr,
                             dump_expr, &info, "", "", ", ", "");
        coutput("%s  if( %s != %s(%s) ) {\n"
                "%s    dague_remote_dep_memcpy( %s(%s), g%s, %s );\n"
                "%s  }\n",                
                spaces, f->varname, call->func_or_mem, string_arena_get_string(sa),
                spaces, call->func_or_mem, string_arena_get_string(sa), f->varname, 
                datatype == NULL ? "DAGUE_DEFAULT_DATA_TYPE" : datatype,
                spaces);
    }

    string_arena_free(sa);
    string_arena_free(sa2);
}

static void jdf_generate_code_flow_final_writes(const jdf_t *jdf, const char *fname, const jdf_dataflow_t *f)
{
    jdf_dep_list_t *dl;
    expr_info_t info;
    string_arena_t *sa;

    sa = string_arena_new(64);
    info.sa = sa;
    info.prefix = "";

    for(dl = f->deps; dl != NULL; dl = dl->next) {
        if( dl->dep->type == JDF_DEP_TYPE_IN )
            /** No final write for input-only flows */
            continue;

        switch( dl->dep->guard->guard_type ) {
        case JDF_GUARD_UNCONDITIONAL:
            if( dl->dep->guard->calltrue->var == NULL ) {
                jdf_generate_code_call_final_write( jdf, dl->dep->guard->calltrue, dl->dep->datatype, f->lineno, fname, f, "" );
            }
            break;
        case JDF_GUARD_BINARY:
            if( dl->dep->guard->calltrue->var == NULL ) {
                coutput("  if( %s ) {\n",
                        dump_expr((void**)&dl->dep->guard->guard, &info));
                jdf_generate_code_call_final_write( jdf, dl->dep->guard->calltrue, dl->dep->datatype, f->lineno, fname, f, "  " );
                coutput("  }\n");
            }
            break;
        case JDF_GUARD_TERNARY:
            if( dl->dep->guard->calltrue->var == NULL ) {
                coutput("  if( %s ) {\n",
                        dump_expr((void**)&dl->dep->guard->guard, &info));
                jdf_generate_code_call_final_write( jdf, dl->dep->guard->calltrue, dl->dep->datatype, f->lineno, fname, f, "  " );
                if( dl->dep->guard->callfalse->var == NULL ) {
                    coutput("  } else {\n");
                    jdf_generate_code_call_final_write( jdf, dl->dep->guard->callfalse, dl->dep->datatype, f->lineno, fname, f, "  " );
                    coutput("  }\n");
                }
            } else if ( dl->dep->guard->callfalse->var == NULL ) {
                coutput("  if( !(%s) ) {\n",
                        dump_expr((void**)&dl->dep->guard->guard, &info));
                jdf_generate_code_call_final_write( jdf, dl->dep->guard->callfalse, dl->dep->datatype, f->lineno, fname, f, "  " );
                coutput("  }\n");
            }
            break;
        }
    }

    string_arena_free(sa);
}

static void jdf_generate_code_papi_events_before(const jdf_t *jdf, const jdf_function_entry_t *f)
{
    coutput("  /** PAPI events */\n"
            "#if defined(HAVE_PAPI)\n"
            "  {\n"
            "    int i, num_events;\n"
            "    int events[MAX_EVENTS];\n"
            "    PAPI_list_events(eventSet, &events, &num_events);\n"
            "    long long values[num_events];\n"
            "    PAPI_start(eventSet);\n"
            "  }\n"
            "#endif /* HAVE_PAPI */\n");
}

static void jdf_generate_code_papi_events_after(const jdf_t *jdf, const jdf_function_entry_t *f)
{
    coutput("  /** PAPI events */\n"
            "#if defined(HAVE_PAPI)\n"
            "  PAPI_stop(eventSet, &values);\n"
            "  if(num_events > 0) {\n"
            "    printf(\"PAPI counter values from  %s (thread=%%ld): \", context->eu_id);\n"
            "    for(i=0; i<num_events; ++i) {\n"
            "      char event_name[PAPI_MAX_STR_LEN];\n"
            "      PAPI_event_code_to_name(events[i], &event_name);\n"
            "      printf(\"   %%s  %%lld \", event_name, values[i]);\n"
            "    }\n"
            "    printf(\"\\n\");\n"
            "  }\n"
            "#endif /** HAVE_PAPI */\n",
            f->fname);
}

static void jdf_generate_code_grapher_task_done(const jdf_t *jdf, const jdf_function_entry_t *f)
{
    string_arena_t *sa;
    sa = string_arena_new(64);

    coutput("#if defined(DAGUE_GRAPHER)\n"
            "  if( NULL != __dague_graph_file ) {\n"
            "    char tmp[128];\n"
            "    dague_service_to_string(exec_context, tmp, 128);\n"
            "    fprintf(__dague_graph_file,\n"
            "           \"%%s [shape=\\\"polygon\\\",style=filled,fillcolor=\\\"%%s\\\",fontcolor=\\\"black\\\",label=\\\"%%s\\\",tooltip=\\\"%s%%ld\\\"];\\n\",\n"
            "            tmp, colors[context->eu_id], tmp, %s_hash( __dague_object, %s ));\n"
            "  }\n"
            "#endif /* DAGUE_GRAPHER */\n",
            f->fname, f->fname, UTIL_DUMP_LIST_FIELD(sa, f->parameters, next, name,
                                                     dump_string, NULL, "", "", ", ", ""));
    
    string_arena_free(sa);
}

static void jdf_generate_code_cache_awareness_update(const jdf_t *jdf, const jdf_function_entry_t *f)
{
    string_arena_t *sa;
    sa = string_arena_new(64);
    
    coutput("  /** Cache Awareness Accounting */\n"
            "#if defined(DAGUE_CACHE_AWARENESS)\n"
            "%s"
            "#endif /* DAGUE_CACHE_AWARENESS */\n",
            UTIL_DUMP_LIST_FIELD(sa, f->dataflow, next, flow,
                                 dump_dataflow_varname, NULL, 
                                 "", "  cache_buf_referenced(context->closest_cache, ", ");\n", ");\n"));
    
    string_arena_free(sa);
}

static void jdf_generate_code_call_release_dependencies(const jdf_t *jdf, const jdf_function_entry_t *f)
{
    string_arena_t *sa;
    int nboutput = 0;
    jdf_dataflow_list_t *dl;

    sa = string_arena_new(64);

    for(dl = f->dataflow; dl != NULL; dl = dl->next) {
        if( jdf_dataflow_type(dl->flow) & JDF_DEP_TYPE_OUT ) {
            string_arena_add_string(sa, "    data[%d] = g%s;\n", nboutput, dl->flow->varname);
            nboutput++;
        }
    }

    coutput("  {\n"
            "    gc_data_t *data[%d];\n"
            "%s"
            "    release_deps_of_%s_%s(context, exec_context,\n"
            "        DAGUE_ACTION_RELEASE_REMOTE_DEPS |\n"
            "        DAGUE_ACTION_RELEASE_LOCAL_DEPS |\n"
            "        DAGUE_ACTION_DEPS_MASK,\n"
            "        NULL, data);\n"
            "  }\n",
            nboutput, string_arena_get_string(sa), jdf_basename, f->fname);

    string_arena_free(sa);
}

static void jdf_generate_code_call_memory_release(const jdf_t *jdf, const char *fname, const jdf_dataflow_t *f,
                                                  const char *spaces)
{
    coutput("%s  data_repo_entry_used_once( %s_repo, e%s->key );\n"
            "%s  (void)gc_data_unref(g%s);\n",
            spaces, fname, f->varname,
            spaces, f->varname);
}

static void jdf_generate_code_flow_memory_release(const jdf_t *jdf, const char *fname, const jdf_dataflow_t *f)
{
    jdf_dep_list_t *dl;
    expr_info_t info;
    string_arena_t *sa;

    sa = string_arena_new(64);
    info.sa = sa;
    info.prefix = "";

    for(dl = f->deps; dl != NULL; dl = dl->next) {
        if( dl->dep->type == JDF_DEP_TYPE_OUT )
            /** No memory release for output-only flows */
            continue;

        switch( dl->dep->guard->guard_type ) {
        case JDF_GUARD_UNCONDITIONAL:
            if( dl->dep->guard->calltrue->var != NULL ) {
                jdf_generate_code_call_memory_release(jdf, dl->dep->guard->calltrue->func_or_mem, f, "");
            }
            break;
        case JDF_GUARD_BINARY:
            if( dl->dep->guard->calltrue->var != NULL ) {
                coutput("  if( %s ) {\n",
                        dump_expr((void**)&dl->dep->guard->guard, &info));
                jdf_generate_code_call_memory_release(jdf, dl->dep->guard->calltrue->func_or_mem, f, "  ");
                coutput("  }\n");
            }
            break;
        case JDF_GUARD_TERNARY:
            if( dl->dep->guard->calltrue->var != NULL ) {
                if( dl->dep->guard->callfalse->var != NULL ) {
                    jdf_generate_code_call_memory_release(jdf, dl->dep->guard->callfalse->func_or_mem, f, "");
                } else {
                    coutput("  if( %s ) {\n",
                            dump_expr((void**)&dl->dep->guard->guard, &info));
                    jdf_generate_code_call_memory_release(jdf, dl->dep->guard->calltrue->func_or_mem, f, "  ");
                    coutput("  }\n");
                }
            } else if ( dl->dep->guard->callfalse->var != NULL ) {
                coutput("  if( !(%s) ) {\n",
                        dump_expr((void**)&dl->dep->guard->guard, &info));
                jdf_generate_code_call_memory_release(jdf, dl->dep->guard->callfalse->func_or_mem, f, "  ");
                coutput("  }\n");
            }
            break;
        }
    }

    string_arena_free(sa);
}

static void jdf_generate_code_hook(const jdf_t *jdf, const jdf_function_entry_t *f, const char *name)
{
    string_arena_t *sa, *sa2;
    assignment_info_t ai;
    jdf_dataflow_list_t *fl;
    int ls, rs;
    int di;

    sa  = string_arena_new(64);
    sa2 = string_arena_new(64);
    ai.sa = sa2;
    ai.idx = 0;
    ai.holder = "exec_context->locals";
    ai.expr = NULL;
    coutput("static int %s(dague_execution_unit_t *context, dague_execution_context_t *exec_context)\n"
            "{\n"
            "  __dague_%s_internal_object_t *__dague_object = (__dague_%s_internal_object_t *)exec_context->dague_object;\n"
            "%s\n",
            name, jdf_basename, jdf_basename,
            UTIL_DUMP_LIST_FIELD(sa, f->definitions, next, name, 
                                 dump_assignments, &ai, "", "  int ", "", ""));
    coutput("  /** Declare the variables that will hold the data, and all the accounting for each */\n"
            "%s\n",
            UTIL_DUMP_LIST_FIELD(sa, f->dataflow, next, flow,
                                 dump_data_declaration, sa2, "", "", "", ""));
    
    coutput("  /** Lookup the input data, and store them in the context */\n");
    for( di = 0, fl = f->dataflow; fl != NULL; fl = fl->next, di++ ) {
        jdf_generate_code_flow_initialization(jdf, f->fname, fl->flow);
        coutput("  exec_context->data[%d].gc_data = g%s;\n"
                "  exec_context->data[%d].data_repo = e%s;\n",
                di, fl->flow->varname,
                di, fl->flow->varname);
    }

    jdf_generate_code_papi_events_before(jdf, f);

    jdf_generate_code_cache_awareness_update(jdf, f);

    jdf_coutput_prettycomment('-', "%s BODY", f->fname);
    coutput("  TAKE_TIME(context, %s_start_key, %s_hash( __dague_object, %s ));\n",
            f->fname, f->fname,
            UTIL_DUMP_LIST_FIELD(sa, f->parameters, next, name,
                                 dump_string, NULL, "", "", ", ", ""));
    coutput("%s\n"
            "#line %d \"%s\"\n",
            f->body,
            cfile_lineno + 2 + nblines(f->body), jdf_cfilename);
    jdf_coutput_prettycomment('-', "END OF %s BODY", f->fname);
    coutput("  TAKE_TIME(context, %s_end_key, %s_hash( __dague_object, %s ));\n",
            f->fname, f->fname,
            UTIL_DUMP_LIST_FIELD(sa, f->parameters, next, name,
                                 dump_string, NULL, "", "", ", ", ""));

    jdf_generate_code_papi_events_after(jdf, f);

    coutput("#if defined(DISTRIBUTED)\n"
            "  /** If not working on distributed, there is no risk that data is not in place */\n");
    for( fl = f->dataflow; fl != NULL; fl = fl->next ) {
        jdf_generate_code_flow_final_writes(jdf, f->fname, fl->flow);
    }
    coutput("#endif /* DISTRIBUTED */\n");

    jdf_generate_code_grapher_task_done(jdf, f);

    jdf_generate_code_call_release_dependencies(jdf, f);

    coutput("  /** Release the hash tables entries of the input data */\n");
    for( fl = f->dataflow; fl != NULL; fl = fl->next ) {
        jdf_generate_code_flow_memory_release(jdf, f->fname, fl->flow);
    }

    coutput("  return 0;\n"
            "}\n\n");
    string_arena_free(sa);
    string_arena_free(sa2);
}

static void jdf_generate_code_release_deps(const jdf_t *jdf, const jdf_function_entry_t *f, const char *name)
{
    string_arena_t *sa = string_arena_new(64);
    string_arena_t *sa1 = string_arena_new(64);
    assignment_info_t ai;

    ai.sa = sa;
    ai.idx = 0;
    ai.holder = "context->locals";
    ai.expr = NULL;

    coutput("static int %s(dague_execution_unit_t *eu, dague_execution_context_t *context, int action_mask, dague_remote_deps_t *deps, gc_data_t **data)\n"
            "{\n"
            "  const __dague_%s_internal_object_t *__dague_object = (const __dague_%s_internal_object_t *)context->dague_object;\n"
            "  dague_release_dep_fct_arg_t arg;\n"
            "%s"
            "  arg.nb_released = 0;\n"
            "  arg.output_usage = 0;\n"
            "  arg.action_mask = action_mask;\n"
            "  arg.deps = deps;\n"
            "  arg.data = data;\n"
            "  arg.ready_list = NULL;\n"
            "\n",
            name, jdf_basename, jdf_basename,
            UTIL_DUMP_LIST_FIELD(sa1, f->definitions, next, name, 
                                       dump_assignments, &ai, "", "  int ", "", ""));

    coutput("  if( action_mask & DAGUE_ACTION_RELEASE_LOCAL_DEPS ) {\n"
            "    arg.output_entry = data_repo_lookup_entry_and_create( %s_repo, %s_hash(__dague_object, %s) );\n"
            "  }\n",
            f->fname, f->fname, 
            UTIL_DUMP_LIST_FIELD(sa, f->parameters, next, name,
                                 dump_string, NULL, "", "", ", ", ""));
    
    coutput("#if defined(DISTRIBUTED)\n"
            "  arg.remote_deps_count = 0;\n"
            "  arg.remote_deps = NULL;\n"
            "  if(action_mask & DAGUE_ACTION_INIT_REMOTE_DEPS) {\n"
            "    arg.root = __dague_object->super.%s->myrank;\n"
            "  }\n"
            "#endif\n",
            f->predicate->func_or_mem);

    coutput("  iterate_successors_of_%s_%s(eu, context, dague_release_dep_fct, &arg);\n"
            "\n",
            jdf_basename, f->fname);

    coutput("  if(action_mask & DAGUE_ACTION_RELEASE_LOCAL_DEPS) {\n"
            "    data_repo_entry_addto_usage_limit(%s_repo, arg.output_entry->key, arg.output_usage);\n"
            "    if( NULL != arg.ready_list ) {\n"
            "      __dague_schedule(context, arg.ready_list, !(DAGUE_ACTION_NO_PLACEHOLDER & action_mask));\n"
            "    }\n"
            "  }\n",
            f->fname);

    coutput("#if defined(DISTRIBUTED)\n"
            "  if( (action_mask & DAGUE_ACTION_SEND_REMOTE_DEPS) && arg.remote_deps_count ) {\n"
            "    arg.nb_released += dague_remote_dep_activate(eu, context, arg.remote_deps, arg.remote_deps_count);\n"
            "  }\n"
            "#endif\n"
            "\n"
            "  return arg.nb_released;\n"
            "}\n"
            "\n");

    string_arena_free(sa);
    string_arena_free(sa1);
}

static char *jdf_dump_context_assignment(string_arena_t *sa_open, const jdf_t *jdf, const char *calltext,
                                         const jdf_call_t *call, int lineno, const char *prefix, const char *var)
{
    jdf_function_entry_t *t;
    jdf_expr_list_t *el;
    jdf_name_list_t *nl;
    int i;
    expr_info_t info, linfo;
    string_arena_t *sa2;
    string_arena_t *sa1;
    string_arena_t *sa_close;
    int nbopen;
    jdf_dataflow_list_t *tf;
    char *p;

    string_arena_init(sa_open);

    for(t = jdf->functions; t != NULL; t = t->next) 
        if( !strcmp(t->fname, call->func_or_mem) )
            break;

    if( NULL == t ) {
         jdf_fatal(lineno, 
                   "During code generation: unable to find function %s referenced in this call.\n",
                   call->func_or_mem);
         exit(1);
    }

    sa1 = string_arena_new(64);
    sa2 = string_arena_new(64);
    
    p = (char*)malloc(strlen(t->fname) + 2);
    sprintf(p, "%s_", t->fname);

    linfo.prefix = p;
    linfo.sa = sa1;

    info.sa = sa2;
    info.prefix = "";

    sa_close = string_arena_new(64);

    nbopen = 0;
    for(el = call->parameters, nl = t->parameters, i = 0; 
        el != NULL && nl != NULL; 
        el = el->next, nl = nl->next, i++) {
        
        string_arena_add_string(sa_open, 
                                "%s%*s{\n"
                                "%s%*s  int %s_%s;\n",
                                prefix, nbopen, "  ",
                                prefix, nbopen, "  ", t->fname, nl->name);
        string_arena_add_string(sa_close,
                                "%s%*s}\n", prefix, nbopen, "  ");

        if( el->expr->op == JDF_RANGE ) {
            string_arena_add_string(sa_open, "%s%*s  for( %s_%s = %s;",
                                    prefix, nbopen, "  ", t->fname, nl->name, dump_expr((void**)&el->expr->jdf_ba1, &info));
            string_arena_add_string(sa_open, " %s_%s <= %s; %s_%s++ ) {\n",
                                    t->fname, nl->name, dump_expr((void**)&el->expr->jdf_ba2, &info), t->fname, nl->name);
            string_arena_add_string(sa_close,
                                    "%s%*s}\n", prefix, nbopen, "  ");
            nbopen++;
        } else {
            string_arena_add_string(sa_open, "%s%*s  %s_%s = %s;\n", prefix, nbopen, "  ", t->fname, nl->name, dump_expr((void**)&el->expr, &info));
        }
        jdf_def_list_t *def;
        
        for(def = t->definitions; NULL != def; def = def->next)
            if( !strcmp(def->name, nl->name) )
                break;
        
        if( def == NULL ) {
            jdf_fatal(lineno, "During code generation: parameter %s of function %s has no definition ?!?\n",
                      nl->name, t->fname);
            exit(1);
        }
        
        if( def->expr->op == JDF_RANGE ) {
            string_arena_add_string(sa_open, 
                                    "%s%*s  if( (%s_%s >= (%s))", 
                                    prefix, nbopen, "  ", t->fname, nl->name, 
                                    dump_expr((void**)&def->expr->jdf_ba1, &linfo));
            string_arena_add_string(sa_open, " && (%s_%s <= (%s)) ) {\n",
                                    t->fname, nl->name, 
                                    dump_expr((void**)&def->expr->jdf_ba2, &linfo));
            string_arena_add_string(sa_close, "%s%*s}\n", prefix,nbopen, "  ");
            nbopen++;
        } else {
            string_arena_add_string(sa_open, 
                                    "%s%*s  if( (%s_%s == (%s))", 
                                    prefix, nbopen, "  ", t->fname, nl->name, 
                                    dump_expr((void**)&def->expr, &linfo));
            string_arena_add_string(sa_close, "%s%*s}\n", prefix,nbopen, "  ");
            nbopen++;
        }
        
        string_arena_add_string(sa_open, "%s%*s  %s.locals[%d].value = %s_%s;\n", 
                                prefix, nbopen, "  ", var, i, 
                                t->fname, nl->name);
        
        nbopen++;
    }
    
    string_arena_add_string(sa_open, "%s%*s  rank_dst =__dague_object->super.%s->rank_of(__dague_object->super.%s, %s);\n",
                            prefix, nbopen, "  ", t->predicate->func_or_mem, t->predicate->func_or_mem,
                            UTIL_DUMP_LIST_FIELD(sa2, t->predicate->parameters, next, expr,
                                                 dump_expr, &linfo,
                                                 "", "", ", ", ""));
    free(p);
    linfo.prefix = NULL;

    string_arena_add_string(sa_open, "%s%*s  %s.function = (const dague_t*)&%s_%s;\n",
                            prefix, nbopen, "  ", var, jdf_basename, t->fname);

    string_arena_add_string(sa_open, "%s%*s  %s.priority = priority_of_%s_%s_as_expr_fct(exec_context->dague_object, nc.locals);\n",
                            prefix, nbopen, "  ", var, jdf_basename, t->fname);
    
    string_arena_add_string(sa_open, 
                            "%s%*s  if( %s == DAGUE_ITERATE_STOP )\n"
                            "%s%*s    return;\n"
                            "\n",
                            prefix, nbopen, "  ", calltext,
                            prefix, nbopen, "  ");

    string_arena_add_string(sa_open, "%s\n", string_arena_get_string(sa_close));

    string_arena_free(sa_close);
    string_arena_free(sa2);

    if( (void*)el != (void*)nl) {
        jdf_fatal(lineno,
                  "During code generation: call to %s at this line has not the same number of parameters as the function definition.\n",
                  call->func_or_mem);
        exit(1);        
    }

    return string_arena_get_string(sa_open);
}

static void jdf_generate_code_iterate_successors(const jdf_t *jdf, const jdf_function_entry_t *f, const char *name)
{
    jdf_dataflow_list_t *fl;
    jdf_dep_list_t *dl;
    int flowempty, flowtomem, noflows;
    string_arena_t *sa = string_arena_new(64);
    string_arena_t *sa1 = string_arena_new(64);
    string_arena_t *sa2 = string_arena_new(64);
    string_arena_t *sa_src = string_arena_new(64);
    int flownb, depnb;
    assignment_info_t ai;
    expr_info_t info;

    info.sa = sa2;
    info.prefix = "";

    ai.sa = sa2;
    ai.idx = 0;
    ai.holder = "exec_context->locals";
    ai.expr = NULL;
    coutput("static void %s(dague_execution_unit_t *eu, dague_execution_context_t *exec_context,\n"
            "               dague_ontask_function_t *ontask, void *ontask_arg)\n"
            "{\n"
            "  dague_execution_context_t nc;\n"
            "  int rank_src, rank_dst;\n"
            "  const __dague_%s_internal_object_t *__dague_object = (const __dague_%s_internal_object_t*)exec_context->dague_object;\n"
            "%s\n",
            name,
            jdf_basename, jdf_basename,
            UTIL_DUMP_LIST_FIELD(sa1, f->definitions, next, name, 
                                       dump_assignments, &ai, "", "  int ", "", ""));

    coutput("  memcpy(&nc, exec_context, sizeof(dague_execution_context_t));\n");

    flownb = 0;
    for(fl = f->dataflow; fl != NULL; fl = fl->next) {
        coutput("  /* Flow of Data %s */\n", fl->flow->varname);
        flowempty = 1;
        flowtomem = 0;

        depnb = 0;
        for(dl = fl->flow->deps; dl != NULL; dl = dl->next) {
            if( dl->dep->type & JDF_DEP_TYPE_OUT )  {

                string_arena_init(sa_src);
                string_arena_add_string(sa_src, "  rank_src =__dague_object->super.%s->rank_of(__dague_object->super.%s, %s);\n",
                                        f->predicate->func_or_mem, f->predicate->func_or_mem,
                                        UTIL_DUMP_LIST_FIELD(sa, f->predicate->parameters, next, expr,
                                                             dump_expr, &info,
                                                             "", "", ", ", ""));

                string_arena_init(sa);
                string_arena_add_string(sa, "ontask(eu, &nc, exec_context, %d, %d, rank_src, rank_dst, ontask_arg)",
                                        flownb, depnb);

                switch( dl->dep->guard->guard_type ) {
                case JDF_GUARD_UNCONDITIONAL:
                    if( NULL != dl->dep->guard->calltrue->var) {
                        flowempty = 0;
                        
                        coutput("%s\n"
                                "%s\n",
                                string_arena_get_string(sa_src),
                                jdf_dump_context_assignment(sa1, jdf, string_arena_get_string(sa), dl->dep->guard->calltrue, dl->dep->lineno, 
                                                            "  ", "nc") );
                    } else {
                        flowtomem = 1;
                    }
                    break;
                case JDF_GUARD_BINARY:
                    if( NULL != dl->dep->guard->calltrue->var ) {
                        flowempty = 0;
                        coutput("  if( %s ) {\n"
                                "  %s\n"
                                "%s\n"
                                "  }\n",
                                dump_expr((void**)&dl->dep->guard->guard, &info),
                                string_arena_get_string(sa_src),
                                jdf_dump_context_assignment(sa1, jdf, string_arena_get_string(sa), dl->dep->guard->calltrue, dl->dep->lineno, 
                                                            "    ", "nc") );
                    } else {
                        flowtomem = 1;
                    }
                    break;
                case JDF_GUARD_TERNARY:
                    if( NULL != dl->dep->guard->calltrue->var ) {
                        flowempty = 0;
                        coutput("  if( %s ) {\n"
                                "  %s\n"
                                "%s\n"
                                "  }",
                                dump_expr((void**)&dl->dep->guard->guard, &info),
                                string_arena_get_string(sa_src),
                                jdf_dump_context_assignment(sa1, jdf, string_arena_get_string(sa), dl->dep->guard->calltrue, dl->dep->lineno, 
                                                            "    ", "nc"));

                        depnb++;
                        string_arena_init(sa);
                        string_arena_add_string(sa, "ontask(eu, &nc, exec_context, %d, %d, rank_src, rank_dst, ontask_arg)",
                                        flownb, depnb);

                        if( NULL != dl->dep->guard->callfalse->var ) {
                            coutput(" else {\n"
                                    "  %s\n"
                                    "%s\n"
                                    "  }\n",
                                    string_arena_get_string(sa_src),
                                    jdf_dump_context_assignment(sa1, jdf, string_arena_get_string(sa), dl->dep->guard->callfalse, dl->dep->lineno, 
                                                                "    ", "nc") );
                        } else {
                            coutput("\n");
                        }
                    } else {
                        depnb++;
                        string_arena_init(sa);
                        string_arena_add_string(sa, "ontask(eu, &nc, exec_context, %d, %d, rank_src, rank_dst, ontask_arg)",
                                        flownb, depnb);

                        if( NULL != dl->dep->guard->callfalse->var ) {
                            flowempty = 0;
                            coutput("  if( !(%s) ) {\n"
                                    "  %s\n"
                                    "%s\n"
                                    "  }\n",
                                    dump_expr((void**)&dl->dep->guard->guard, &info),
                                    string_arena_get_string(sa_src),
                                    jdf_dump_context_assignment(sa1, jdf, string_arena_get_string(sa), dl->dep->guard->callfalse, dl->dep->lineno, 
                                                                "    ", "nc") );
                        } else {
                            flowtomem = 1;
                        }
                    }
                    break;
                }
                depnb++;
            }
        }

        if( (1 == flowempty) && (0 == flowtomem) ) {
            coutput("  /* This flow has only IN dependencies */\n");
        } else if( 1 == flowempty ) {
            coutput("  /* This flow has only OUTPUT dependencies to Memory */\n");
            flownb++;
        } else {
            flownb++;
        }
        coutput("\n");
    }

    coutput("}\n"
            "\n");

    string_arena_free(sa);
    string_arena_free(sa1);
    string_arena_free(sa2);
    string_arena_free(sa_src);
}


/** Main Function */

int jdf2c(const char *output_c, const char *output_h, const char *_jdf_basename, const jdf_t *jdf)
{
    int ret = 0;

    init_unique_rgb_color();

    jdf_cfilename = output_c;
    jdf_basename = _jdf_basename;
    cfile = NULL;
    hfile = NULL;

    cfile = fopen(output_c, "w");
    if( cfile == NULL ) {
        fprintf(stderr, "unable to create %s: %s\n", output_c, strerror(errno));
        ret = -1;
        goto err;
    }

    hfile = fopen(output_h, "w");
    if( hfile == NULL ) {
        fprintf(stderr, "unable to create %s: %s\n", output_h, strerror(errno));
        ret = -1;
        goto err;
    }

    cfile_lineno = 1;
    hfile_lineno = 1;
    
    jdf_generate_header_file(jdf);
    jdf_generate_structure(jdf);
    jdf_generate_hashfunctions(jdf);
    jdf_generate_predeclarations( jdf );
    jdf_generate_functions_statics(jdf);

    /**
     * Generate the externally visible function.
     */
    jdf_generate_constructor(jdf);
 err:
    if( NULL != cfile ) 
        fclose(cfile);

    if( NULL != hfile )
        fclose(hfile);

    return ret;
}
