/**
 * @file symtable.c
 * @brief Implementation of Symtable ADT
 * @author Filip Radulovič (xradulf00)
 */

#include "symtable.h"
#include "hashtable.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define INITIAL_HTAB_CAPACITY 8

// entry of the scope stack, doesnt own anything (owned by function.locals)
typedef struct Scope {
    Htab* vars;
    struct Scope* parent;
    StabFunc* func;
} Scope;

struct Stab {
    Htab* funcs;
    StabFunc* main;
    Scope* current_scope; // top of scope stack
};

// key for function table
// name is owned by function.name
typedef struct {
    const char* name;
    bool is_extension;
    DataType extends_type;
} StabFuncKey;

// djb2 hash function used for variables and function names
static HtabSize djb2(const unsigned char* s)
{
    HtabSize hash = 5381;
    int c;
    while ((c = *s++)) {
        hash = ((hash << 5) + hash) + c;
    }

    return hash;
}

// hash function for StabFuncKey
// extends type is only mixed in for methods
static HtabSize func_hash(HtabKey key)
{
    const StabFuncKey* k = key;
    HtabSize hash = djb2((const unsigned char*)k->name);
    if (k->is_extension) {
        hash = (hash << 5) + hash;
        hash += (HtabSize)k->extends_type + 1;
    }
    return hash;
}

// wrapper for string hash
static HtabSize str_hash(HtabKey key)
{
    return djb2(key);
}

// checks if two stab_funcs_key are equal
static bool eq_fn(HtabKey a, HtabKey b)
{
    const StabFuncKey* x = a;
    const StabFuncKey* y = b;
    if (strcmp(x->name, y->name) != 0) {
        return false;
    }
    if (x->is_extension != y->is_extension) {
        return false;
    }
    if (x->is_extension && x->extends_type != y->extends_type) {
        return false;
    }
    return true;
}

// value free function for function table
// frees all values
static void value_free_fn(void* ptr)
{
    StabFunc* f = ptr;
    if (!f) {
        return;
    }

    for (StabSize i = 0; i < f->list_count; i++) {
        for (StabSize j = 0; j < f->param_lists[i].count; j++) {
            free((void*)f->param_lists[i].params[j].name);
        }
        free(f->param_lists[i].params);
    }
    free(f->param_lists);

    for (StabSize i = 0; i < f->locals_count; i++) {
        free((void*)f->locals[i]->name);
        free(f->locals[i]);
    }
    free(f->locals);

    free((void*)f->extends.name);
    free((void*)f->name);
    free(f);
}

// strdup (c11 has problems with strdup, this replaces it)
static char* str_copy(const char* s)
{
    if (!s) {
        return NULL;
    }
    StabSize n = strlen(s) + 1;
    char* t = malloc(n);
    if (!t) {
        return NULL;
    }
    memcpy(t, s, n);
    return t;
}

// checks if two strings are equal
static bool str_eq(HtabKey a, HtabKey b)
{
    const char* s1 = a;
    const char* s2 = b;
    int res = strcmp(s1, s2);
    return res == 0;
}

// copies param list from src to dst, return true if sucessful, otherwise false
static bool copy_param_list(StabParamList* dst, const StabParamList* src)
{
    if (src->count == 0) {
        return true;
    }

    dst->params = calloc(src->count, sizeof *dst->params);
    if (!dst->params) {
        return false;
    }
    dst->count = src->count;

    for (StabSize i = 0; i < src->count; i++) {
        dst->params[i].id = src->params[i].id;
        dst->params[i].type = src->params[i].type;
        dst->params[i].name = str_copy(src->params[i].name);
        if (!dst->params[i].name) {
            return false;
        }
    }

    return true;
}

// copies every param list using copy_param_list, return true if sucessfull, otherwise false
static bool copy_all_param_lists(StabFunc* f, const StabParamList* param_lists, StabSize list_count)
{
    if (list_count == 0) {
        return true;
    }

    f->param_lists = calloc(list_count, sizeof *f->param_lists);
    if (!f->param_lists) {
        return false;
    }
    f->list_count = list_count;

    for (StabSize i = 0; i < list_count; i++) {
        if (!copy_param_list(&f->param_lists[i], &param_lists[i])) {
            return false;
        }
    }

    return true;
}

StabFunc* stab_add_func(Stab* st,
    const char* name,
    const StabParamList* param_lists,
    StabSize list_count,
    DataType ret_type,
    bool ret_explicit,
    const StabParam* extends,
    StabRes* res)
{

    StabFunc* f = calloc(1, sizeof *f);
    if (!f) {
        *res = ST_NOMEM;
        return NULL;
    }

    f->ret_type = ret_type;
    f->ret_explicit = ret_explicit;
    f->infer = INFER_NONE;
    f->is_extension = extends != NULL;

    f->name = str_copy(name);
    if (!f->name) {
        value_free_fn(f);
        *res = ST_NOMEM;
        return NULL;
    }

    if (!copy_all_param_lists(f, param_lists, list_count)) {
        value_free_fn(f);
        *res = ST_NOMEM;
        return NULL;
    }

    if (extends) {
        f->extends.name = str_copy(extends->name);
        if (!f->extends.name) {
            value_free_fn(f);
            *res = ST_NOMEM;
            return NULL;
        }
        f->extends.type = extends->type;
    }

    StabFuncKey* key = malloc(sizeof *key);
    if (!key) {
        value_free_fn(f);
        *res = ST_NOMEM;
        return NULL;
    }
    key->name = f->name;
    key->is_extension = f->is_extension;
    key->extends_type = f->extends.type;

    HtabRes hi = htab_insert(st->funcs, key, f);
    if (hi != HTAB_OK) {
        free(key);
        value_free_fn(f);
        if (hi == HTAB_EXISTS) {
            *res = ST_REDEFINED;
            return NULL;
        }
        *res = ST_NOMEM;
        return NULL;
    }

    *res = ST_OK;
    return f;
}

static const StabFunc BUILTINS[] = {
    { .name = "print",
        .builtin = BI_PRINT,
        .ret_type = TYPE_UNIT,
        .param_lists = (StabParamList[]) { { (StabParam[]) { { "value", TYPE_ANY, 0 } }, 1 } },
        .list_count = 1 },
    { .name = "readLine",
        .builtin = BI_READLINE,
        .ret_type = TYPE_STRING,
        .param_lists = (StabParamList[]) { { NULL, 0 } },
        .list_count = 1 },
    { .name = "readInt",
        .builtin = BI_READINT,
        .ret_type = TYPE_INT,
        .param_lists = (StabParamList[]) { { NULL, 0 } },
        .list_count = 1 },
    { .name = "readDouble",
        .builtin = BI_READDOUBLE,
        .ret_type = TYPE_DOUBLE,
        .param_lists = (StabParamList[]) { { NULL, 0 } },
        .list_count = 1 },
    { .name = "readBoolean",
        .builtin = BI_READBOOLEAN,
        .ret_type = TYPE_BOOL, // BOOLTHEN
        .param_lists = (StabParamList[]) { { NULL, 0 } },
        .list_count = 1 },
    { .name = "toStr",
        .builtin = BI_TOSTR,
        .ret_type = TYPE_STRING,
        .param_lists = (StabParamList[]) { { (StabParam[]) { { "value", TYPE_ANY, 0 } }, 1 } },
        .list_count = 1 },
    { .name = "toInt",
        .builtin = BI_TOINT,
        .ret_type = TYPE_INT,
        .param_lists = (StabParamList[]) { { (StabParam[]) { { "value", TYPE_DOUBLE, 0 } }, 1 } },
        .list_count = 1 },
    { .name = "strLength",
        .builtin = BI_STRLENGTH,
        .ret_type = TYPE_INT,
        .param_lists = (StabParamList[]) { { (StabParam[]) { { "value", TYPE_STRING, 0 } }, 1 } },
        .list_count = 1 },
    { .name = "subStr",
        .builtin = BI_SUBSTR,
        .ret_type = TYPE_STRING,
        .param_lists = (StabParamList[]) {
            { (StabParam[]) { { "value", TYPE_STRING, 0 }, { "start", TYPE_INT, 0 }, { "end", TYPE_INT, 0 } }, 3 } },
        .list_count = 1 },
    { .name = "ord", .builtin = BI_ORD, .ret_type = TYPE_INT, .param_lists = (StabParamList[]) { { (StabParam[]) { { "value", TYPE_STRING, 0 }, { "index", TYPE_INT, 0 } }, 2 } }, .list_count = 1 }, { .name = "chr", .builtin = BI_CHR, .ret_type = TYPE_STRING, .param_lists = (StabParamList[]) { { (StabParam[]) { { "code", TYPE_INT, 0 } }, 1 } }, .list_count = 1 }, { .name = "toString", .builtin = BI_M_TOSTRING, .ret_type = TYPE_STRING, .is_extension = true, .extends = { "value", TYPE_ANY, 0 } }, { .name = "toInt", .builtin = BI_M_TOINT, .ret_type = TYPE_INT, .is_extension = true, .extends = { "value", TYPE_DOUBLE, 0 } }, { .name = "floor", .builtin = BI_M_FLOOR, .ret_type = TYPE_DOUBLE, .is_extension = true, .extends = { "value", TYPE_DOUBLE, 0 } }, { .name = "length", .builtin = BI_M_LENGTH, .ret_type = TYPE_INT, .is_extension = true, .extends = { "value", TYPE_STRING, 0 } }, { .name = NULL }
};

// return false on ST_NOMEM error
static bool register_builtin_fns(Stab* st)
{
    for (StabSize i = 0; BUILTINS[i].name != NULL; i++) {

        const StabParam* pr_extends = NULL;
        if (BUILTINS[i].is_extension) {
            pr_extends = &BUILTINS[i].extends;
        }

        StabRes res;
        StabFunc* f = stab_add_func(st,
            BUILTINS[i].name,
            BUILTINS[i].param_lists,
            BUILTINS[i].list_count,
            BUILTINS[i].ret_type,
            true,
            pr_extends,
            &res);
        if (f == NULL || res != ST_OK) {
            return false;
        }

        f->is_builtin = true;
        f->builtin = BUILTINS[i].builtin;
        f->infer = INFER_DONE;
    }

    return true;
}

Stab* stab_new()
{
    Stab* st = malloc(sizeof *st);
    if (st == NULL) {
        return NULL;
    }

    st->main = NULL;
    st->current_scope = NULL;

    st->funcs = htab_init(INITIAL_HTAB_CAPACITY, func_hash, eq_fn, free, value_free_fn);
    if (st->funcs == NULL) {
        free(st);
        return NULL;
    }

    if (!register_builtin_fns(st)) {
        htab_destroy(st->funcs);
        free(st);
        return NULL;
    }

    return st;
}

void stab_free(Stab* st)
{
    if (!st) {
        return;
    }

    while (st->current_scope != NULL) {
        stab_leave_scope(st);
    }

    htab_destroy(st->funcs);

    free(st);
}

StabRes stab_set_main(Stab* st, StabFunc* fn)
{
    assert(st != NULL);
    if (st->main != NULL) {
        return ST_REDEFINED;
    }

    st->main = fn;
    fn->is_main = true;
    return ST_OK;
}

StabFunc* stab_main(const Stab* st)
{
    assert(st != NULL);
    return st->main;
}

// used for: while for etc. inherits the function from its parent
StabRes stab_enter_scope(Stab* st)
{
    assert(st != NULL && st->current_scope != NULL);
    Scope* s = malloc(sizeof *s);
    if (!s) {
        return ST_NOMEM;
    }

    s->vars = htab_init(INITIAL_HTAB_CAPACITY, str_hash, str_eq, NULL, NULL);
    if (!s->vars) {
        free(s);
        return ST_NOMEM;
    }
    s->func = st->current_scope->func;
    s->parent = st->current_scope;
    st->current_scope = s;
    return ST_OK;
}

StabRes stab_enter_func(Stab* st, StabFunc* fn)
{
    assert(st != NULL && fn != NULL);
    Scope* s = malloc(sizeof *s);
    if (!s) {
        return ST_NOMEM;
    }

    s->vars = htab_init(INITIAL_HTAB_CAPACITY, str_hash, str_eq, NULL, NULL);
    if (!s->vars) {
        free(s);
        return ST_NOMEM;
    }
    s->func = fn;
    s->parent = st->current_scope;
    st->current_scope = s;
    return ST_OK;
}

void stab_leave_scope(Stab* st)
{
    if (!st || !st->current_scope) {
        return;
    }
    Scope* s = st->current_scope;
    st->current_scope = s->parent;
    htab_destroy(s->vars);
    free(s);
}

void stab_leave_func(Stab* st)
{
    if (!st || !st->current_scope) {
        return;
    }
    StabFunc* fn = st->current_scope->func;
    while (st->current_scope && st->current_scope->func == fn) {
        stab_leave_scope(st);
    }
}

StabFunc* stab_current_func(const Stab* st)
{
    if (!st || !st->current_scope) {
        return NULL;
    }

    return st->current_scope->func;
}

// allocates variable and adds it to function.locals
static StabVar* add_local(StabFunc* f, const char* name, DataType type, bool is_mutable, bool is_param)
{
    StabVar* v = malloc(sizeof *v);
    if (!v) {
        return NULL;
    }

    v->name = str_copy(name);
    if (!v->name) {
        free(v);
        return NULL;
    }

    v->type = type;
    v->is_mutable = is_mutable;
    v->is_param = is_param;
    v->id = f->locals_count;

    StabVar** tmp = realloc(f->locals, (f->locals_count + 1) * sizeof *tmp);
    if (!tmp) {
        free((void*)v->name);
        free(v);
        return NULL;
    }

    f->locals = tmp;
    f->locals[f->locals_count++] = v;
    return v;
}

StabVar* stab_declare_var(Stab* st, const char* name, DataType type, bool is_mutable, StabRes* res, bool is_param)
{
    assert(st != NULL && st->current_scope != NULL && st->current_scope->func != NULL);

    StabFunc* f = st->current_scope->func;

    if (htab_get(st->current_scope->vars, name, NULL)) {
        *res = ST_REDEFINED;
        return NULL;
    }

    StabVar* v = add_local(f, name, type, is_mutable, is_param);
    if (!v) {
        *res = ST_NOMEM;
        return NULL;
    }

    HtabRes rh = htab_insert(st->current_scope->vars, v->name, v);

    if (rh == HTAB_OK) {
        *res = ST_OK;
        return v;
    }

    f->locals_count--;
    free((void*)v->name);
    free(v);
    *res = ST_NOMEM;
    return NULL;
}

StabVar* stab_find_var(const Stab* st, const char* name)
{
    assert(st != NULL);
    for (const Scope* s = st->current_scope; s != NULL && s->func == st->current_scope->func; s = s->parent) {
        HtabValue out;
        if (htab_get(s->vars, name, &out)) {
            return out;
        }
    }
    return NULL;
}

StabFunc* stab_find_func(const Stab* st, const char* name, const DataType* extends)
{
    assert(st != NULL && name != NULL);

    StabFuncKey key = {
        .name = name,
        .is_extension = extends != NULL,
        .extends_type = extends ? *extends : TYPE_UNKNOWN,
    };

    HtabValue out;
    if (htab_get(st->funcs, &key, &out)) {
        return out;
    }

    if (extends && *extends != TYPE_ANY) {
        key.extends_type = TYPE_ANY;
        if (htab_get(st->funcs, &key, &out)) {
            return out;
        }
    }

    return NULL;
}
