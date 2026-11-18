/**
 * @file semantic.c
 * @brief Implementation of Semantic analysis
 * @author Filip Radulovič (xradulf00)
 * @todo Add comments
 */

#include "semantic.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static stab *sem_st = NULL;

// prints error message to stderr and returns sem. error code
static sem_res_t error(sem_res_t code, unsigned int line, const char *msg) {
    fprintf(stderr, "Semantic error (%d)", code);
    if (line != 0) {
        fprintf(stderr, " on line %u", line);
    }
    fprintf(stderr, ": %s\n", msg);
    return code;
}

// error code convertor (from stab to sem)
static sem_res_t stab_res_convert(stab_res_t res) {
    switch (res) {
    case ST_OK:
        return SEM_OK;
    case ST_REDEFINED:
        return SEM_REDEFINED;
    case ST_NOMEM:
    default:
        return SEM_INTERNAL;
    }
}

// creates param list in the same format as stab needs
static sem_res_t build_param_list(const AstNode *function, stab_param_list *out) {
    const AstNodeList *ast_params = &function->as.function.parameters;

    out->params = NULL;
    out->count = ast_params->count;

    // no need to check anything
    if (ast_params->count == 0) {
        return SEM_OK;
    }

    // same structure use by stab
    out->params = malloc(ast_params->count * sizeof(*out->params));
    if (!out->params) {
        return SEM_INTERNAL;
    }

    for (size_t i = 0; i < ast_params->count; i++) {
        const AstNode *param = ast_params->items[i];
        assert(param->node_kind == AST_PARAMETER);

        out->params[i].name = param->as.parameter.name;
        out->params[i].type = param->as.parameter.type;
    }

    return SEM_OK;
}

static sem_res_t check_main_sig(const AstNode *fn) {
    if (fn->as.function.parameters.count != 0) {
        return error(SEM_BAD_CALL, fn->line, "@main function cannot have parameters");
    }

    DataType ret = fn->as.function.return_type;
    if (ret != TYPE_UNKNOWN && ret != TYPE_UNIT) {
        return error(SEM_TYPE, fn->line, "@main function must return Unit");
    }

    return SEM_OK;
}

static sem_res_t declare_fn(AstNode *fn) {
    assert(fn->node_kind == AST_FUNCTION);

    // malloc in build_param_list
    stab_param_list list;
    sem_res_t res = build_param_list(fn, &list);
    if (res != SEM_OK) {
        return res;
    }

    DataType ret_type = fn->as.function.return_type;
    bool ret_explicit;
    if (ret_type == TYPE_UNKNOWN) {
        ret_explicit = false;
    } else {
        ret_explicit = true;
    }

    stab_res_t res2;
    // list_count = 1 -> without any addons (different number would be introduced by PARAMLISTS addon)
    stab_func *f = stab_add_func(sem_st, fn->as.function.name, &list, 1, ret_type, ret_explicit, NULL, &res2);
    // now lives in stab, we can safely free it
    free(list.params);

    if (!f) {
        if (res2 == ST_REDEFINED) {
            return error(SEM_REDEFINED, fn->line, "redefinition of function");
        }
        return stab_res_convert(res2);
    }

    // second pass needs ast_node reference
    f->ast_node = fn;

    if (fn->as.function.is_main_func) {
        // reusing res is safe here
        res = check_main_sig(fn);
        if (res != SEM_OK) {
            return res;
        }

        if (stab_set_main(sem_st, f) == ST_REDEFINED) {
            return error(SEM_REDEFINED, fn->line, "more than one @main function");
        }
    }

    return SEM_OK;
}

// first pass, collect all functions and variables
static sem_res_t collect_fns(const AstNode *program) {
    assert(program->node_kind == AST_PROGRAM);

    const AstNodeList *functions = &program->as.program.functions;
    size_t fns_len = functions->count;

    for (size_t i = 0; i < fns_len; i++) {
        sem_res_t res = declare_fn(functions->items[i]);

        if (res != SEM_OK) {
            return res;
        }
    }

    if (stab_main(sem_st) == NULL) {
        return error(SEM_UNDEFINED, 0, "missing @main function");
    }

    return SEM_OK;
}

static sem_res_t declare_params(AstNode *fn) {
    const AstNodeList *params = &fn->as.function.parameters;
    size_t params_len = params->count;

    for (size_t i = 0; i < params_len; i++) {
        AstNode *param = params->items[i];

        stab_res_t res;
        stab_var *v = stab_declare_var(sem_st, param->as.parameter.name, param->as.parameter.type, false, &res);
        if (!v) {
            if (res == ST_REDEFINED) {
                return error(SEM_REDEFINED, param->line, "duplicate parameter name");
            }
            return stab_res_convert(res);
        }

        // for codegen
        param->var_id = v->id;
    }

    return SEM_OK;
}

static sem_res_t check_fn(stab_func *fn) {
    AstNode *ast = fn->ast_node;

    sem_res_t res = stab_res_convert(stab_enter_func(sem_st, fn));
    if (res != SEM_OK) {
        return res;
    }

    res = declare_params(ast);
    if (res != SEM_OK) {
        return res;
    }

    stab_leave_func(sem_st);
    return SEM_OK;
}

// second pass, checks bodies of all functions
static sem_res_t check_fns(const AstNode *program) {
    const AstNodeList *functions = &program->as.program.functions;
    size_t fn_len = functions->count;

    for (size_t i = 0; i < fn_len; i++) {
        stab_func *fn = stab_find_func(sem_st, functions->items[i]->as.function.name, NULL);

        sem_res_t res = check_fn(fn);
        if (res != SEM_OK) {
            return res;
        }
    }

    return SEM_OK;
}

void semantic_init(stab *st) {
    assert(st != NULL);
    sem_st = st;
}

sem_res_t semantic_analyse(AstNode *program) {
    assert(sem_st != NULL && program != NULL);

    sem_res_t res = collect_fns(program);
    if (res != SEM_OK) {
        return res;
    }

    return check_fns(program);
}
