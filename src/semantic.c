/**
 * @file semantic.c
 * @brief Implementation of Semantic analysis
 * @author Filip Radulovič (xradulf00)
 * @todo Add support for PARAMLISTS in the first pass (waiting for ast)
 * @todo Add support for the rest of extensions
 */

#include "semantic.h"
#include "ast.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

// global symtable pointer
static stab *sem_st = NULL;

// stack like function checking chain for finding recursive cycles
typedef struct fn_chain {
    const stab_func *fn;
    const struct fn_chain *prev;
} fn_chain;

// top of function checking chain
static const fn_chain *checking = NULL;

// Returns true if type is INT or DOUBLE.
static bool is_numeric(DataType type) {
    return type == TYPE_INT || type == TYPE_DOUBLE;
}

// Returns whether source can be assigned to target.
static bool is_assignable(DataType target, DataType source) {
    if (target == source || target == TYPE_ANY) {
        return true;
    }
    return target == TYPE_DOUBLE && source == TYPE_INT;
}

// Returns whether fn has explicit return type TYPE_UNIT.
static bool is_explicit_unit(const stab_func *fn) {
    return fn->ret_explicit && fn->ret_type == TYPE_UNIT;
}

// Prints error message to stderr and returns semantic error code.
static sem_res_t error(sem_res_t code, unsigned int line, const char *msg) {
    fprintf(stderr, "Semantic error (%d)", code);
    if (line != 0) {
        fprintf(stderr, " on line %u", line);
    }
    fprintf(stderr, ": %s\n", msg);
    return code;
}

// Converts symtable error codes to semantic error codes and returns the result.
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

// Returns last statement from block.
static const AstNode *last_statement(const AstNode *block) {
    const AstNodeList *statements = &block->as.block.statements;
    return statements->items[statements->count - 1];
}

// Checks that the main function has a valid signature.
// Returns SEM_OK if signature valid, otherwise a semantic error code.
static sem_res_t check_main_signature(const AstNode *function) {
    // main function cannot have any parameters
    if (function->as.function.parameters.count != 0) {
        return error(SEM_BAD_CALL, function->line, "@main function cannot have parameters");
    }

    // main function has to have Unit return type (or none specified)
    DataType return_type = function->as.function.return_type;
    if (return_type != TYPE_UNKNOWN && return_type != TYPE_UNIT) {
        return error(SEM_TYPE, function->line, "@main function must return Unit");
    }

    return SEM_OK;
}

// Frees parameter lists created by build_param_lists().
static void free_param_lists(stab_param_list *lists, size_t count) {
    if (!lists) {
        return;
    }
    for (size_t i = 0; i < count; i++) {
        free(lists[i].params);
    }
    free(lists);
}

// Builds parameter lists for symtable from given function AST node.
// Returns SEM_OK if successful, otherwise SEM_INTERNAL (memory allocation failed).
static sem_res_t build_param_lists(const AstNode *function, stab_param_list **out, size_t *out_count) {
    const AstNodeList *parameters = &function->as.function.parameters;
    size_t list_count = function->as.function.list_count;
    const size_t *list_sizes = function->as.function.list_sizes;

    *out = NULL;
    *out_count = list_count;

    // no need to allocate anything
    if (list_count == 0) {
        assert(parameters->count == 0);
        return SEM_OK;
    }

    stab_param_list *lists = calloc(list_count, sizeof(*lists));
    if (!lists) {
        return SEM_INTERNAL;
    }

    // assert rather than segfault if parser makes a mistake
    assert(list_sizes != NULL);

    size_t param_idx = 0;
    for (size_t list_idx = 0; list_idx < list_count; list_idx++) {
        size_t size = list_sizes[list_idx];
        lists[list_idx].count = size;

        // empty list
        if (size == 0) {
            continue;
        }

        lists[list_idx].params = malloc(size * sizeof(*lists[list_idx].params));
        if (!lists[list_idx].params) {
            free_param_lists(lists, list_count);
            return SEM_INTERNAL;
        }

        // copy name and type of every parameter in this list
        for (size_t i = 0; i < size; i++) {
            const AstNode *param = parameters->items[param_idx];
            assert(param->node_kind == AST_PARAMETER);

            lists[list_idx].params[i].name = param->as.parameter.name;
            lists[list_idx].params[i].type = param->as.parameter.type;
            // assigned later by declare_params()
            lists[list_idx].params[i].id = 0;

            param_idx++;
        }
    }

    // list_sizes has to cover all flattened parameters exactly
    assert(param_idx == parameters->count);

    *out = lists;
    return SEM_OK;
}

// Declares function in the global symtable, builds parameter lists.
// If function is marked as main, checks signature and registers it.
// Returns SEM_OK on success, otherwise semantic error code.
static sem_res_t declare_function(AstNode *fn) {
    assert(fn->node_kind == AST_FUNCTION);

    // allocate param_lists
    stab_param_list *lists;
    size_t list_count;
    sem_res_t res = build_param_lists(fn, &lists, &list_count);
    if (res != SEM_OK) {
        return res;
    }

    // return type is explicit unless its UNKNOWN
    DataType ret_type = fn->as.function.return_type;
    bool ret_explicit;
    if (ret_type == TYPE_UNKNOWN) {
        ret_explicit = false;
    } else {
        ret_explicit = true;
    }

    stab_res_t st_res;

    const AstNode *receiver = fn->as.function.receiver;
    stab_param extends_param;
    const stab_param *extends = NULL;
    if (receiver != NULL) {
        extends_param.name = receiver->as.parameter.name;
        extends_param.type = receiver->as.parameter.type;
        // assigned later in second pass
        extends_param.id = 0;
        extends = &extends_param;
    }
    stab_func *f =
        stab_add_func(sem_st, fn->as.function.name, lists, list_count, ret_type, ret_explicit, extends, &st_res);
    // now lives in stab, we can safely free it
    free_param_lists(lists, list_count);

    if (!f) {
        if (st_res == ST_REDEFINED) {
            return error(SEM_REDEFINED, fn->line, "redefinition of function");
        }
        return stab_res_convert(st_res);
    }

    // second pass needs ast_node reference
    f->ast_node = fn;

    if (fn->as.function.is_main_func) {
        // reusing res is safe here
        res = check_main_signature(fn);
        if (res != SEM_OK) {
            return res;
        }

        if (stab_set_main(sem_st, f) == ST_REDEFINED) {
            return error(SEM_REDEFINED, fn->line, "more than one @main function");
        }
    }

    return SEM_OK;
}

// Collects and declares all functions and its parameters.
// Returns SEM_OK if sucessful, otherwise error code from declare_function.
static sem_res_t collect_all_functions(const AstNode *program) {
    assert(program->node_kind == AST_PROGRAM);

    const AstNodeList *functions = &program->as.program.functions;

    // declare every function
    for (size_t i = 0; i < functions->count; i++) {
        sem_res_t res = declare_function(functions->items[i]);

        if (res != SEM_OK) {
            // calling error() is handled by declare_function
            return res;
        }
    }

    return SEM_OK;
}

// forward declarations
static sem_res_t check_nested_block(AstNode *block);
static sem_res_t check_function(stab_func *fn);
static sem_res_t check_expr(AstNode *node);

// Checks unary operator (-x +x), operand must be a number.
// Returns SEM_OK if successful, otherwise error code.
static sem_res_t check_unary(AstNode *node) {
    AstNode *operand = node->as.unary.operand;

    sem_res_t res = check_expr(operand);
    if (res != SEM_OK) {
        return res;
    }

    // logical negation
    if (node->as.unary.operator_type == OP_NOT) {
        if (operand->type != TYPE_BOOL) {
            return error(SEM_TYPE, node->line, "operator ! requires Boolean");
        }
        node->type = TYPE_BOOL;
        return SEM_OK;
    }

    // operand must be a number
    if (!is_numeric(operand->type)) {
        return error(SEM_TYPE, node->line, "unary operator requires a number");
    }

    node->type = operand->type;
    return SEM_OK;
}

// Checks binary operator from left to right.
// Implicitly convert Int to Double when needed.
// Returns SEM_OK if successful, otherwise error code.
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

    // both operands are numbers
    bool numbers = is_numeric(l) && is_numeric(r);
    // both operands are strings
    bool strings = l == TYPE_STRING && r == TYPE_STRING;

    switch (node->as.binary.operator_type) {
    case OP_ADD:
        // + on strings is concatenation
        if (strings) {
            node->type = TYPE_STRING;
            return SEM_OK;
        }
        // intentional fall through to arithmetic
    case OP_SUB:
    case OP_MUL:
    case OP_DIV:
        if (!numbers) {
            return error(SEM_TYPE, node->line, "arithmetic operator requires numbers");
        }

        // implicit conversion to Double when needed
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
        // throw error when incomparable
        if (!numbers && !strings) {
            return error(SEM_TYPE, node->line, "relational operator requires two numbers or two strings");
        }

        node->type = TYPE_BOOL;

        return SEM_OK;

    case OP_EQ:
    case OP_NE:
        // ensure the same type for both operands
        if (!numbers && l != r) {
            return error(SEM_TYPE, node->line, "equality operator requires same types or two numbers");
        }

        node->type = TYPE_BOOL;

        return SEM_OK;

    case OP_AND:
    case OP_OR:
        // logical operators for BOOLTHEN
        if (l != TYPE_BOOL || r != TYPE_BOOL) {
            return error(SEM_TYPE, node->line, "logical operator requires two Booleans");
        }

        node->type = TYPE_BOOL;

        return SEM_OK;

    default:
        return error(SEM_INTERNAL, node->line, "unsupported operator");
    }
}

// Checks a function call and infers return type for normal function.
// Recursive call require explicit return type.
// Returns SEM_OK if successful, otherwise error code.
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

    // left to right
    for (size_t i = 0; i < args->count; i++) {
        sem_res_t res = check_expr(args->items[i]);
        if (res != SEM_OK) {
            return res;
        }

        if (!is_assignable(params->params[i].type, args->items[i]->type)) {
            return error(SEM_BAD_CALL, node->line, "wrong type of argument");
        }
    }

    // callee is being checked = recursion
    // every function in the cycle needs explicit return type
    if (f->infer == INFER_RUNNING) {
        for (const fn_chain *c = checking;; c = c->prev) {
            if (!c->fn->ret_explicit) {
                return error(SEM_TYPE, node->line, "recursive function must have explicit return type");
            }
            if (c->fn == f) {
                break;
            }
        }
        // callee not checked yet
    } else if (f->infer == INFER_NONE) {
        sem_res_t res = check_function(f);
        if (res != SEM_OK) {
            return res;
        }
    }

    node->type = f->ret_type;

    return SEM_OK;
}

// Checks identifier of a variable and sets type and id.
// Returns SEM_OK if successful, otherwise error code.
static sem_res_t check_identifier(AstNode *node) {
    stab_var *v = stab_find_var(sem_st, node->as.identifier.name);
    if (!v) {
        return error(SEM_UNDEFINED, node->line, "undefined variable");
    }

    node->type = v->type;
    node->var_id = v->id;

    return SEM_OK;
}

// Checks expression and sets its node type and var id.
// Returns SEM_OK if successful, otherwise error code.
static sem_res_t check_expr(AstNode *node) {
    switch (node->node_kind) {
    case AST_LITERAL:
        // no need to check, type already set by parser
        return SEM_OK;

    case AST_IDENTIFIER:
        return check_identifier(node);

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

// Checks variable definition, infers type if needed.
// Returns SEM_OK if successful, otherwise error code.
static sem_res_t check_variable_definition(AstNode *node) {
    AstNode *value = node->as.variable_definition.value;

    sem_res_t res = check_expr(value);
    if (res != SEM_OK) {
        return res;
    }

    DataType type = node->as.variable_definition.declared_type;
    // not a declared type = infer it
    if (type == TYPE_UNKNOWN) {
        type = value->type;
        // declared type, value must match type
    } else if (!is_assignable(type, value->type)) {
        return error(SEM_TYPE, node->line, "incompatible initialization of variable");
    }

    stab_res_t st_res;
    stab_var *v = stab_declare_var(sem_st,
                                   node->as.variable_definition.name,
                                   type,
                                   node->as.variable_definition.is_mutable,
                                   &st_res,
                                   false);
    if (!v) {
        if (st_res == ST_REDEFINED) {
            return error(SEM_REDEFINED, node->line, "redefinition of variable");
        }
        return stab_res_convert(st_res);
    }

    node->var_id = v->id;

    return SEM_OK;
}

// Checks assignment to an existing variable, which must be mutable.
// Returns SEM_OK if successful, otherwise error code.
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

    // is type and value assignable?
    if (!is_assignable(v->type, value->type)) {
        return error(SEM_TYPE, node->line, "incompatible type in assignment");
    }

    node->var_id = v->id;

    return SEM_OK;
}

// Checks condition of if / while, which must be boolean.
// Returns SEM_OK if successful, otherwise error code.
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

// Checks return statement.
// Returns SEM_OK if successful, otherwise error code.
static sem_res_t check_return(AstNode *node) {
    const stab_func *fn = stab_current_func(sem_st);
    AstNode *value = node->as.return_statement.value;

    // bare return returns (), only allowed in explicit Unit
    if (!value) {
        if (!is_explicit_unit(fn)) {
            return error(SEM_OTHER, node->line, "return without value is allowed only in Unit function");
        }

        return SEM_OK;
    }

    // all returns require explicit return type
    if (!fn->ret_explicit) {
        return error(SEM_TYPE, node->line, "function with return must have explicit return type");
    }

    sem_res_t res = check_expr(value);
    if (res != SEM_OK) {
        return res;
    }

    // explicit Unit throws away return value
    // otherwise type must match value
    if (!is_explicit_unit(fn) && !is_assignable(fn->ret_type, value->type)) {
        return error(SEM_TYPE, node->line, "incompatible return value");
    }

    node->type = value->type;
    return SEM_OK;
}

// Checks expression from expression statement.
// Returns SEM_OK if successful, otherwise error code.
static sem_res_t check_expression_statement(AstNode *node) {
    sem_res_t res = check_expr(node->as.expression_statement.expression);
    if (res != SEM_OK) {
        return res;
    }

    node->type = node->as.expression_statement.expression->type;

    return SEM_OK;
}

// Checks if statement, condition must be boolean.
// Each branch has its own scope.
// Returns SEM_OK if successful, otherwise error code.
static sem_res_t check_if(AstNode *node) {
    sem_res_t res = check_cond(node->as.if_statement.condition);
    if (res != SEM_OK) {
        return res;
    }
    // separate scope for each branch
    res = check_nested_block(node->as.if_statement.then_block);
    if (res != SEM_OK) {
        return res;
    }
    // else if is stored as else block with nested if
    if (node->as.if_statement.else_block) {
        return check_nested_block(node->as.if_statement.else_block);
    }
    return SEM_OK;
}

// Checks while loop, condition must be boolean.
// Body has its own scope.
// Returns SEM_OK if successful, otherwise error code.
static sem_res_t check_while(AstNode *node) {
    sem_res_t res = check_cond(node->as.while_statement.condition);
    if (res != SEM_OK) {
        return res;
    }
    return check_nested_block(node->as.while_statement.block);
}

// Checks a statement by calling corresponding check function.
// Returns SEM_OK if successful, otherwise error code.
static sem_res_t check_statement(AstNode *node) {
    // statements without value result in Unit
    // expression statements and return overwrite it with the correct type
    node->type = TYPE_UNIT;

    switch (node->node_kind) {
    case AST_VARIABLE_DEFINITION:
        return check_variable_definition(node);

    case AST_ASSIGN:
        return check_assign(node);

    case AST_RETURN:
        return check_return(node);

    case AST_EXPRESSION_STATEMENT:
        return check_expression_statement(node);

    case AST_IF:
        return check_if(node);

    case AST_WHILE:
        return check_while(node);

    default:
        return error(SEM_INTERNAL, node->line, "unexpected node in block");
    }
}

// Checks all statements in a block in order.
// Returns SEM_OK if successful, otherwise error code.
static sem_res_t check_all_statements(AstNode *block) {
    assert(block->node_kind == AST_BLOCK);

    const AstNodeList *statements = &block->as.block.statements;

    for (size_t i = 0; i < statements->count; i++) {
        sem_res_t res = check_statement(statements->items[i]);
        if (res != SEM_OK) {
            return res;
        }
    }

    return SEM_OK;
}

// Checks neste block in its own scope.
// Returns SEM_OK if successful, otherwise error code.
static sem_res_t check_nested_block(AstNode *block) {
    sem_res_t res = stab_res_convert(stab_enter_scope(sem_st));
    if (res != SEM_OK) {
        return res;
    }

    res = check_all_statements(block);
    if (res != SEM_OK) {
        return res;
    }

    stab_leave_scope(sem_st);
    return SEM_OK;
}

// Returns true if the block ends with return in every branch.
static bool all_branches_return(const AstNode *block) {
    const AstNode *last = last_statement(block);

    if (last->node_kind != AST_IF) {
        return last->node_kind == AST_RETURN;
    }

    const AstNode *then_block = last->as.if_statement.then_block;
    const AstNode *else_block = last->as.if_statement.else_block;

    if (else_block == NULL) {
        return false;
    }

    if (!all_branches_return(then_block)) {
        return false;
    }

    return all_branches_return(else_block);
}
// Returns true if the block ends with Unit in every branch.
static bool all_branches_unit(const AstNode *block) {
    const AstNode *last = last_statement(block);

    if (last->node_kind != AST_IF) {
        return last->type == TYPE_UNIT;
    }

    const AstNode *then_block = last->as.if_statement.then_block;
    const AstNode *else_block = last->as.if_statement.else_block;

    if (!all_branches_unit(then_block)) {
        return false;
    }

    if (else_block == NULL) {
        return true;
    }

    return all_branches_unit(else_block);
}

// Returns true if the function body may end with if.
static bool if_at_end_allowed(const stab_func *fn, const AstNode *body) {
    if (is_explicit_unit(fn)) {
        return true;
    }

    if (all_branches_return(body)) {
        return true;
    }

    if (!fn->ret_explicit && all_branches_unit(body)) {
        return true;
    }

    return false;
}

// Declares all parameters of a function as immutable variables.
// Returns SEM_OK if successful, otherwise error code.
static sem_res_t declare_params(AstNode *fn, stab_func *f) {
    const AstNodeList *parameters = &fn->as.function.parameters;

    size_t param_idx = 0;

    for (size_t list_idx = 0; list_idx < f->list_count; list_idx++) {
        stab_param_list *list = &f->param_lists[list_idx];

        for (size_t i = 0; i < list->count; i++) {
            AstNode *param = parameters->items[param_idx];

            stab_res_t res;
            stab_var *var =
                stab_declare_var(sem_st, param->as.parameter.name, param->as.parameter.type, false, &res, true);
            if (!var) {
                if (res == ST_REDEFINED) {
                    return error(SEM_REDEFINED, param->line, "duplicate parameter name");
                }
                return stab_res_convert(res);
            }

            // save id for codegen
            param->var_id = var->id;
            list->params[i].id = var->id;

            param_idx++;
        }
    }
    return SEM_OK;
}

// Checks body of a function and infers its return type.
// The function is marked INFER_RUNNING while being checked, so check_call() can detect recursion,
// and its chain link lets check_call verify that every function has explicit return type.
// Returns SEM_OK if successful, otherwise semantic error code.
static sem_res_t check_function(stab_func *fn) {
    AstNode *ast = fn->ast_node;

    // push chain of currently being checked functions
    fn_chain link = {fn, checking};
    checking = &link;
    fn->infer = INFER_RUNNING;

    sem_res_t res = stab_res_convert(stab_enter_func(sem_st, fn));
    if (res != SEM_OK) {
        return res;
    }

    // declare params as immutable variables
    res = declare_params(ast, fn);
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

    res = check_all_statements(body);
    if (res != SEM_OK) {
        return res;
    }

    // result of the body is given by its last statement
    const AstNodeList *statements = &body->as.block.statements;
    const AstNode *last = statements->items[statements->count - 1];

    // ending with if is allowed only for:
    //  - explicit Unit
    //  - return in every branch
    //  - return type Unit in every branch
    if (last->node_kind == AST_IF && !if_at_end_allowed(fn, body)) {
        return error(SEM_TYPE, last->line, "function body ending with if must return in every branch");
    }

    // infer return type from last one
    if (!fn->ret_explicit) {
        fn->ret_type = last->type;
        // return type doesnt match allowed return types
    } else if (!is_explicit_unit(fn) && last->node_kind != AST_RETURN && last->node_kind != AST_IF) {
        if (!is_assignable(fn->ret_type, last->type)) {
            return error(SEM_TYPE, last->line, "result of function body does not match return type");
        }
    }

    // main fn has to end with Unit return type
    if (fn->is_main && fn->ret_type != TYPE_UNIT) {
        return error(SEM_TYPE, ast->line, "@main function must return Unit");
    }

    // pop chain
    // no invalid recursion found by check_call()
    fn->infer = INFER_DONE;
    checking = link.prev;

    stab_leave_func(sem_st);
    return SEM_OK;
}

// Check bodies of all functions.
// Returns SEM_OK if successful, otherwise error code.
static sem_res_t check_all_functions(const AstNode *program) {
    const AstNodeList *functions = &program->as.program.functions;

    for (size_t i = 0; i < functions->count; i++) {
        stab_func *func = stab_find_func(sem_st, functions->items[i]->as.function.name, NULL);

        // infer done, skip this function
        if (func->infer != INFER_NONE) {
            continue;
        }

        sem_res_t res = check_function(func);
        if (res != SEM_OK) {
            // error() calling is handled by check_function()
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

    // first pass: collect all function signatures and declare them
    sem_res_t res = collect_all_functions(program);
    if (res != SEM_OK) {
        return res;
    }

    // after first pass program must have valid main function
    if (stab_main(sem_st) == NULL) {
        return error(SEM_UNDEFINED, 0, "missing @main function");
    }

    // second pass: check bodies of every function, verify statements, expression, conditions etc.
    return check_all_functions(program);
}
