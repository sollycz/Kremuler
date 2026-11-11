#include "symtable.h"
#include "hashtable.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define INITIAL_HTAB_CAPACITY 8

typedef struct scope {
    htab *vars;
    struct scope *parent;
    stab_func *func;
} scope;

struct stab {
    htab *funcs;
    stab_func *main;
    scope *top;
};

typedef struct {
    const char *name;
    bool is_method;
    stab_type_t receiver_type;
} stab_func_key;

static htab_size_t djb2(const unsigned char *s) {
    htab_size_t hash = 5381;
    int c;
    while ((c = *s++)) {
        hash = ((hash << 5) + hash) + c;
    }

    return hash;
}

static htab_size_t func_hash(htab_key_t key) {
    const stab_func_key *k = key;
    htab_size_t hash = djb2((const unsigned char *)k->name);
    if (k->is_method) {
        hash = (hash << 5) + hash;
        hash += (htab_size_t)k->receiver_type + 1;
    }
    return hash;
}

static htab_size_t str_hash(htab_key_t key) {
    return djb2(key);
}

static bool eq_fn(htab_key_t a, htab_key_t b) {
    const stab_func_key *x = a;
    const stab_func_key *y = b;
    if (strcmp(x->name, y->name) != 0) {
        return false;
    }
    if (x->is_method != y->is_method) {
        return false;
    }
    if (x->is_method && x->receiver_type != y->receiver_type) {
        return false;
    }
    return true;
}

static void value_free_fn(void *ptr) {
    stab_func *f = ptr;
    if (!f) {
        return;
    }

    for (stab_size_t i = 0; i < f->list_count; i++) {
        for (stab_size_t j = 0; j < f->param_lists[i].count; j++) {
            free((void *)f->param_lists[i].params[j].name);
        }
        free(f->param_lists[i].params);
    }
    free(f->param_lists);

    for (stab_size_t i = 0; i < f->locals_count; i++) {
        free((void *)f->locals[i]->name);
        free(f->locals[i]);
    }
    free(f->locals);

    free((void *)f->receiver.name);
    free((void *)f->name);
    free(f);
}

static char *str_copy(const char *s) {
    if (!s) {
        return NULL;
    }
    stab_size_t n = strlen(s) + 1;
    char *t = malloc(n);
    if (!t) {
        return NULL;
    }
    memcpy(t, s, n);
    return t;
}

static bool str_eq(htab_key_t a, htab_key_t b) {
    const char *s1 = a;
    const char *s2 = b;
    int res = strcmp(s1, s2);
    return res == 0;
}

// copies param list from src to dst, return true if sucessfull, otherwise false
static bool copy_param_list(stab_param_list *dst, const stab_param_list *src) {
    if (src->count == 0) {
        return true;
    }

    dst->params = calloc(src->count, sizeof *dst->params);
    if (!dst->params) {
        return false;
    }
    dst->count = src->count;

    for (stab_size_t i = 0; i < src->count; i++) {
        dst->params[i].type = src->params[i].type;
        dst->params[i].name = str_copy(src->params[i].name);
        if (!dst->params[i].name) {
            return false;
        }
    }

    return true;
}

// copies every param list using copy_param_list, return true if sucessfull, otherwise false
static bool copy_all_param_lists(stab_func *f, const stab_param_list *param_lists, stab_size_t list_count) {
    if (list_count == 0) {
        return true;
    }

    f->param_lists = calloc(list_count, sizeof *f->param_lists);
    if (!f->param_lists) {
        return false;
    }
    f->list_count = list_count;

    for (stab_size_t i = 0; i < list_count; i++) {
        if (!copy_param_list(&f->param_lists[i], &param_lists[i])) {
            return false;
        }
    }

    return true;
}

stab_func *stab_add_func(stab *st,
                         const char *name,
                         const stab_param_list *param_lists,
                         stab_size_t list_count,
                         stab_type_t ret_type,
                         bool ret_explicit,
                         const stab_param *receiver,
                         stab_res_t *res) {

    stab_func *f = calloc(1, sizeof *f);
    if (!f) {
        *res = ST_NOMEM;
        return NULL;
    }

    f->ret_type = ret_type;
    f->ret_explicit = ret_explicit;
    if (ret_explicit) {
        f->infer = INFER_DONE;
    } else {
        f->infer = INFER_NONE;
    }
    f->is_method = receiver != NULL;

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

    if (receiver) {
        f->receiver.name = str_copy(receiver->name);
        if (!f->receiver.name) {
            value_free_fn(f);
            *res = ST_NOMEM;
            return NULL;
        }
        f->receiver.type = receiver->type;
    }

    stab_func_key *key = malloc(sizeof *key);
    if (!key) {
        value_free_fn(f);
        *res = ST_NOMEM;
        return NULL;
    }
    key->name = f->name;
    key->is_method = f->is_method;
    key->receiver_type = f->receiver.type;

    htab_res_t hi = htab_insert(st->funcs, key, f);
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

static const stab_func BUILTINS[] = {
    {.name = "print",
     .builtin = BI_PRINT,
     .ret_type = TYPE_UNIT,
     .param_lists = (stab_param_list[]){{(stab_param[]){{"value", TYPE_ANY}}, 1}},
     .list_count = 1},
    {.name = "readLine",
     .builtin = BI_READLINE,
     .ret_type = TYPE_STRING,
     .param_lists = (stab_param_list[]){{NULL, 0}},
     .list_count = 1},
    {.name = "readInt",
     .builtin = BI_READINT,
     .ret_type = TYPE_INT,
     .param_lists = (stab_param_list[]){{NULL, 0}},
     .list_count = 1},
    {.name = "readDouble",
     .builtin = BI_READDOUBLE,
     .ret_type = TYPE_DOUBLE,
     .param_lists = (stab_param_list[]){{NULL, 0}},
     .list_count = 1},
    {.name = "readBoolean",
     .builtin = BI_READBOOLEAN,
     .ret_type = TYPE_BOOL, // BOOLTHEN
     .param_lists = (stab_param_list[]){{NULL, 0}},
     .list_count = 1},
    {.name = "toStr",
     .builtin = BI_TOSTR,
     .ret_type = TYPE_STRING,
     .param_lists = (stab_param_list[]){{(stab_param[]){{"value", TYPE_ANY}}, 1}},
     .list_count = 1},
    {.name = "toInt",
     .builtin = BI_TOINT,
     .ret_type = TYPE_INT,
     .param_lists = (stab_param_list[]){{(stab_param[]){{"value", TYPE_DOUBLE}}, 1}},
     .list_count = 1},
    {.name = "strLength",
     .builtin = BI_STRLENGTH,
     .ret_type = TYPE_INT,
     .param_lists = (stab_param_list[]){{(stab_param[]){{"value", TYPE_STRING}}, 1}},
     .list_count = 1},
    {.name = "subStr",
     .builtin = BI_SUBSTR,
     .ret_type = TYPE_STRING,
     .param_lists =
         (stab_param_list[]){{(stab_param[]){{"value", TYPE_STRING}, {"start", TYPE_INT}, {"end", TYPE_INT}}, 3}},
     .list_count = 1},
    {.name = "ord",
     .builtin = BI_ORD,
     .ret_type = TYPE_INT,
     .param_lists = (stab_param_list[]){{(stab_param[]){{"value", TYPE_STRING}, {"index", TYPE_INT}}, 2}},
     .list_count = 1},
    {.name = "chr",
     .builtin = BI_CHR,
     .ret_type = TYPE_STRING,
     .param_lists = (stab_param_list[]){{(stab_param[]){{"code", TYPE_INT}}, 1}},
     .list_count = 1},
    {.name = "toString",
     .builtin = BI_M_TOSTRING,
     .ret_type = TYPE_STRING,
     .is_method = true,
     .receiver = {"value", TYPE_ANY}},
    {.name = "toInt",
     .builtin = BI_M_TOINT,
     .ret_type = TYPE_INT,
     .is_method = true,
     .receiver = {"value", TYPE_DOUBLE}},
    {.name = "floor",
     .builtin = BI_M_FLOOR,
     .ret_type = TYPE_DOUBLE,
     .is_method = true,
     .receiver = {"value", TYPE_DOUBLE}},
    {.name = "length",
     .builtin = BI_M_LENGTH,
     .ret_type = TYPE_INT,
     .is_method = true,
     .receiver = {"value", TYPE_STRING}},
    {.name = NULL}};

// return false if failed
static bool register_builtin_fns(stab *st) {
    for (stab_size_t i = 0; BUILTINS[i].name != NULL; i++) {

        const stab_param *pr_receiver = NULL;
        if (BUILTINS[i].is_method) {
            pr_receiver = &BUILTINS[i].receiver;
        }

        stab_res_t res;
        stab_func *f = stab_add_func(st,
                                     BUILTINS[i].name,
                                     BUILTINS[i].param_lists,
                                     BUILTINS[i].list_count,
                                     BUILTINS[i].ret_type,
                                     true,
                                     pr_receiver,
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

stab *stab_new() {
    stab *st = malloc(sizeof *st);
    if (st == NULL) {
        return NULL;
    }

    st->main = NULL;
    st->top = NULL;

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

void stab_free(stab *st) {
    if (!st) {
        return;
    }

    while (st->top != NULL) {
        stab_leave_scope(st);
    }

    htab_destroy(st->funcs);

    free(st);
}

stab_res_t stab_set_main(stab *st, stab_func *fn) {
    assert(st != NULL);
    if (st->main != NULL) {
        return ST_REDEFINED;
    }

    st->main = fn;
    fn->is_main = true;
    return ST_OK;
}

stab_func *stab_main(const stab *st) {
    assert(st != NULL);
    return st->main;
}

// used for: while for etc. inherits the function from its parent
stab_res_t stab_enter_scope(stab *st) {
    assert(st != NULL && st->top != NULL);
    scope *s = malloc(sizeof *s);
    if (!s) {
        return ST_NOMEM;
    }

    s->vars = htab_init(INITIAL_HTAB_CAPACITY, str_hash, str_eq, NULL, NULL);
    if (!s->vars) {
        free(s);
        return ST_NOMEM;
    }
    s->func = st->top->func;
    s->parent = st->top;
    st->top = s;
    return ST_OK;
}

stab_res_t stab_enter_func(stab *st, stab_func *fn) {
    assert(st != NULL && fn != NULL);
    scope *s = malloc(sizeof *s);
    if (!s) {
        return ST_NOMEM;
    }

    s->vars = htab_init(INITIAL_HTAB_CAPACITY, str_hash, str_eq, NULL, NULL);
    if (!s->vars) {
        free(s);
        return ST_NOMEM;
    }
    s->func = fn;
    s->parent = st->top;
    st->top = s;
    return ST_OK;
}

void stab_leave_scope(stab *st) {
    if (!st || !st->top) {
        return;
    }
    scope *s = st->top;
    st->top = s->parent;
    htab_destroy(s->vars);
    free(s);
}

void stab_leave_func(stab *st) {
    if (!st || !st->top) {
        return;
    }
    stab_func *fn = st->top->func;
    while (st->top && st->top->func == fn) {
        stab_leave_scope(st);
    }
}

stab_func *stab_current_func(const stab *st) {
    if (!st || !st->top) {
        return NULL;
    }

    return st->top->func;
}
