/**
 * @file semantic.c
 * @brief Implementation of Semantic analysis
 * @author Filip Radulovič (xradulf00)
 * @todo Add comments
 * @todo Rework check_block "double" declaration
 * @todo Same for check_expr and check_fn
 */

#include "semantic.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static stab *sem_st = NULL;

static bool is_numeric(DataType t) {
    return t == TYPE_INT || t == TYPE_DOUBLE;
}

static bool is_assignable(DataType target, DataType src) {
    if (target == src || target == TYPE_ANY) {
        return true;
    }
    return target == TYPE_DOUBLE && src == TYPE_INT;
}

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

// again ugly, but works for now
static sem_res_t check_fn(stab_func *fn);
static sem_res_t check_expr(AstNode *node);

static sem_res_t check_unary(AstNode *node) {
    AstNode *operand = node->as.unary.operand;

    sem_res_t res = check_expr(operand);
    if (res != SEM_OK) {
        return res;
    }

    if (!is_numeric(operand->type)) {
        return error(SEM_TYPE, node->line, "unary operator requires a number");
    }

    node->type = operand->type;
    return SEM_OK;
}

static sem_res_t check_binary(AstNode *node) {
    AstNode *left = node->as.binary.left;
    AstNode *right = node->as.binary.right;

    // left to right
    sem_res_t res = check_expr(left);
    if (res != SEM_OK) {
        return res;
    }

    res = check_expr(right);
    if (res != SEM_OK) {
        return res;
    }

    DataType l = left->type;
    DataType r = right->type;

    bool numbers = is_numeric(l) && is_numeric(r);
    bool strings = l == TYPE_STRING && r == TYPE_STRING;

    switch (node->as.binary.operator_type) {
    case OP_ADD:
        if (strings) {
            node->type = TYPE_STRING;
            return SEM_OK;
        }
        // intentional fall through
    case OP_SUB:
    case OP_MUL:
    case OP_DIV:
        if (!numbers) {
            return error(SEM_TYPE, node->line, "arithmetic operator requires numbers");
        }

        if (l == TYPE_INT && r == TYPE_INT) {
            node->type = TYPE_INT;
        } else {
            node->type = TYPE_DOUBLE;
        }

        return SEM_OK;

    case OP_LT:
    case OP_LE:
    case OP_GT:
    case OP_GE:
        if (!numbers && !strings) {
            return error(SEM_TYPE, node->line, "relational operator requires two numbers or two strings");
        }

        node->type = TYPE_BOOL;

        return SEM_OK;

    case OP_EQ:
    case OP_NE:
        if (!numbers && l != r) {
            return error(SEM_TYPE, node->line, "equality operator requires same types or two numbers");
        }

        node->type = TYPE_BOOL;

        return SEM_OK;

    default:
        // rest is boolthen, not supported for now
        return error(SEM_INTERNAL, node->line, "unsupported operator");
    }
}

static sem_res_t check_call(AstNode *node) {
    stab_func *f = stab_find_func(sem_st, node->as.call.name, NULL);
    if (!f) {
        return error(SEM_UNDEFINED, node->line, "undefined function");
    }

    const stab_param_list *params = &f->param_lists[0];
    const AstNodeList *args = &node->as.call.args;

    if (args->count != params->count) {
        return error(SEM_BAD_CALL, node->line, "wrong number of arguments");
    }

    size_t arg_len = args->count;

    // left to right
    for (size_t i = 0; i < arg_len; i++) {
        sem_res_t res = check_expr(args->items[i]);
        if (res != SEM_OK) {
            return res;
        }

        if (!is_assignable(params->params[i].type, args->items[i]->type)) {
            return error(SEM_BAD_CALL, node->line, "wrong type of argument");
        }
    }

    // recursion has to have explicit return type
    if (f->infer == INFER_RUNNING) {
        return error(SEM_TYPE, node->line, "recursive function must have explicit return type");
    }

    if (f->infer == INFER_NONE) {
        sem_res_t res = check_fn(f);
        if (res != SEM_OK) {
            return res;
        }
    }

    node->type = f->ret_type;

    return SEM_OK;
}

static sem_res_t check_expr(AstNode *node) {
    switch (node->node_kind) {
    case AST_LITERAL:
        // no need to check
        return SEM_OK;

    case AST_IDENTIFIER: {
        stab_var *v = stab_find_var(sem_st, node->as.identifier.name);
        if (!v) {
            return error(SEM_UNDEFINED, node->line, "undefined variable");
        }

        node->type = v->type;
        node->var_id = v->id;

        return SEM_OK;
    }

    case AST_UNARY_OPERATOR:
        return check_unary(node);

    case AST_BINARY_OPERATOR:
        return check_binary(node);

    case AST_CALL:
        return check_call(node);

    default:
        return error(SEM_INTERNAL, node->line, "unexpected node in expression");
    }
}

static sem_res_t check_var_def(AstNode *node) {
    AstNode *value = node->as.variable_definition.value;

    sem_res_t res = check_expr(value);
    if (res != SEM_OK) {
        return res;
    }

    DataType type = node->as.variable_definition.declared_type;
    if (type == TYPE_UNKNOWN) {
        type = value->type;
    } else if (!is_assignable(type, value->type)) {
        return error(SEM_TYPE, node->line, "incompatible initialization of variable");
    }

    stab_res_t st_res;
    stab_var *v = stab_declare_var(sem_st,
                                   node->as.variable_definition.name,
                                   type,
                                   node->as.variable_definition.is_mutable,
                                   &st_res);
    if (!v) {
        if (st_res == ST_REDEFINED) {
            return error(SEM_REDEFINED, node->line, "redefinition of variable");
        }
        return stab_res_convert(st_res);
    }

    node->var_id = v->id;

    return SEM_OK;
}

static sem_res_t check_assign(AstNode *node) {
    stab_var *v = stab_find_var(sem_st, node->as.assign.name);
    if (!v) {
        return error(SEM_UNDEFINED, node->line, "assignment to undefined variable");
    }

    if (!v->is_mutable) {
        return error(SEM_IMMUTABLE, node->line, "assignment to immutable variable");
    }

    AstNode *value = node->as.assign.value;

    sem_res_t res = check_expr(value);
    if (res != SEM_OK) {
        return res;
    }

    if (!is_assignable(v->type, value->type)) {
        return error(SEM_TYPE, node->line, "incompatible type in assignment");
    }

    node->var_id = v->id;

    return SEM_OK;
}

// todo: not a great solution, rework
static sem_res_t check_block(AstNode *block);

static sem_res_t check_cond(AstNode *cond) {
    sem_res_t res = check_expr(cond);
    if (res != SEM_OK) {
        return res;
    }

    if (cond->type != TYPE_BOOL) {
        return error(SEM_TYPE, cond->line, "condition must be Boolean");
    }

    return SEM_OK;
}

static sem_res_t check_statement(AstNode *node) {
    sem_res_t res;

    // without explicit return use TYPE_UNIT
    node->type = TYPE_UNIT;

    switch (node->node_kind) {
    case AST_VARIABLE_DEFINITION:
        return check_var_def(node);

    case AST_ASSIGN:
        return check_assign(node);

    case AST_RETURN:
        if (node->as.return_statement.value) {
            res = check_expr(node->as.return_statement.value);
            if (res != SEM_OK) {
                return res;
            }

            node->type = node->as.return_statement.value->type;
        }

        return SEM_OK;

    case AST_EXPRESSION_STATEMENT:
        res = check_expr(node->as.expression_statement.expression);
        if (res != SEM_OK) {
            return res;
        }

        node->type = node->as.expression_statement.expression->type;

        return SEM_OK;

    case AST_IF:
        res = check_cond(node->as.if_statement.condition);
        if (res != SEM_OK) {
            return res;
        }
        res = check_block(node->as.if_statement.then_block);
        if (res != SEM_OK) {
            return res;
        }
        // else if is stored as else block with nested if
        if (node->as.if_statement.else_block) {
            return check_block(node->as.if_statement.else_block);
        }
        return SEM_OK;

    case AST_WHILE:
        res = check_cond(node->as.while_statement.condition);
        if (res != SEM_OK) {
            return res;
        }
        return check_block(node->as.while_statement.block);

    default:
        return error(SEM_INTERNAL, node->line, "unexpected node in block");
    }
}

static sem_res_t check_block(AstNode *block) {
    assert(block->node_kind == AST_BLOCK);

    sem_res_t res = stab_res_convert(stab_enter_scope(sem_st));
    if (res != SEM_OK) {
        return res;
    }

    const AstNodeList *statements = &block->as.block.statements;
    size_t st_len = statements->count;

    for (size_t i = 0; i < st_len; i++) {
        res = check_statement(statements->items[i]);
        if (res != SEM_OK) {
            return res;
        }
    }

    stab_leave_scope(sem_st);
    return SEM_OK;
}

static sem_res_t check_fn(stab_func *fn) {
    AstNode *ast = fn->ast_node;

    if (!fn->ret_explicit) {
        fn->infer = INFER_RUNNING;
    }

    sem_res_t res = stab_res_convert(stab_enter_func(sem_st, fn));
    if (res != SEM_OK) {
        return res;
    }

    res = declare_params(ast);
    if (res != SEM_OK) {
        return res;
    }

    AstNode *body = ast->as.function.block;

    // 'def f() = expr' is treated as AST_RETURN by parser, even tho it is not
    // solution is to convert it to an AST_EXPRESSION_STATEMENT
    AstNode *first = body->as.block.statements.items[0];
    if (body->line == ast->line && first->node_kind == AST_RETURN) {
        AstNode *value = first->as.return_statement.value;
        first->node_kind = AST_EXPRESSION_STATEMENT;
        first->as.expression_statement.expression = value;
    }

    res = check_block(body);
    if (res != SEM_OK) {
        return res;
    }

    if (!fn->ret_explicit) {
        const AstNodeList *stmts = &body->as.block.statements;
        fn->ret_type = stmts->items[stmts->count - 1]->type;
        fn->infer = INFER_DONE;
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

        if (!fn->ret_explicit && fn->infer != INFER_NONE) {
            continue;
        }

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
