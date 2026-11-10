#include "symtable.h"
#include "hashtable.h"
#include <stdlib.h>

#define INITIAL_HTAB_CAPACITY 8

typedef struct scope {
    htab *vars;
    struct scope *parent;
} scope;

struct stab {
    htab *funcs;
    stab_func *main;
    stab_func *cur_func;
    scope *top;
};

htab_size_t hash_fn(htab_key_t key);
bool eq_fn(htab_key_t a, htab_key_t b);
void key_free_fn(void *ptr);
void value_free_fn(void *ptr);

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
    st->cur_func = NULL;
    st->top = NULL;

    st->funcs = htab_init(8, hash_fn, eq_fn, key_free_fn, value_free_fn);
    if (st->funcs == NULL) {
        free(st);
        return NULL;
    }

    if (!register_builtin_fns(st)) {
        free(st->funcs);
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
