/**
 * @file ast.c
 * @brief Abstract syntax tree of IFJ26
 * @author Oleksandr Teslja (xtesljo00)
 */

#include "ast.h"

#include <stdlib.h>

// creates a new AST node
// (returns NULL on allocation failure)
AstNode *ast_new(AstKind kind, unsigned int line) {
    AstNode *node = calloc(1, sizeof(AstNode));
    if (node == NULL) {
        return NULL;
    }

    node->node_kind = kind;
    node->line = line;

    return node;
}

// adds a node to the end of an AST list
// (returns false on allocation failure)
bool ast_list_push(AstNodeList *list, AstNode *node) {
    AstNode **items = realloc(list->items, (list->count + 1) * sizeof(AstNode *)); // make space for the new node

    if (items == NULL) {
        return false;
    }

    items[list->count] = node;
    list->items = items;
    list->count++;

    return true;
}

// frees the specified AST list and all of its nodes
static void ast_list_free(AstNodeList *list) {
    for (size_t i = 0; i < list->count; i++) {
        ast_free(list->items[i]);
    }

    free(list->items);
    list->items = NULL;
    list->count = 0;
}

static void ast_mark_branch_expression(AstNode *branch) {
    if (branch == NULL) {
        return;
    }

    AstNodeList *statements = &branch->as.block.statements;
    AstNode *last = statements->items[statements->count - 1];
    if (last->node_kind == AST_IF) {
        ast_mark_if_expression(last);
    }
}

// marks the if as an expression, together with the ifs at the end of its branches
void ast_mark_if_expression(AstNode *node) {
    node->as.if_statement.is_expression = true;

    ast_mark_branch_expression(node->as.if_statement.then_block);
    ast_mark_branch_expression(node->as.if_statement.else_block);
}

// frees the specified node and its children based on its kind
// (will free the whole AST if called on the root node)
void ast_free(AstNode *node) {
    if (node == NULL) {
        return;
    }

    // some node kinds have strings/children nodes that need to be freed first
    switch (node->node_kind) {
    case AST_PROGRAM:
        ast_list_free(&node->as.program.functions);
        break;
    case AST_FUNCTION:
        free(node->as.function.name);
        ast_list_free(&node->as.function.parameters);
        free(node->as.function.list_sizes);
        ast_free(node->as.function.block);
        ast_free(node->as.function.receiver);
        break;
    case AST_PARAMETER:
        free(node->as.parameter.name);
        break;
    case AST_BLOCK:
        ast_list_free(&node->as.block.statements);
        break;
    case AST_WHILE:
        ast_free(node->as.while_statement.condition);
        ast_free(node->as.while_statement.block);
        break;
    case AST_FOR:
        free(node->as.for_statement.var_name);
        ast_free(node->as.for_statement.from);
        ast_free(node->as.for_statement.to);
        ast_free(node->as.for_statement.step);
        ast_list_free(&node->as.for_statement.guards);
        ast_free(node->as.for_statement.block);
        break;
    case AST_IF:
        ast_free(node->as.if_statement.condition);
        ast_free(node->as.if_statement.then_block);
        ast_free(node->as.if_statement.else_block);
        break;
    case AST_VARIABLE_DEFINITION:
        free(node->as.variable_definition.name);
        ast_free(node->as.variable_definition.value);
        break;
    case AST_ASSIGN:
        free(node->as.assign.name);
        ast_free(node->as.assign.value);
        break;
    case AST_RETURN:
        ast_free(node->as.return_statement.value);
        break;
    case AST_EXPRESSION_STATEMENT:
        ast_free(node->as.expression_statement.expression);
        break;
    case AST_LITERAL:
        // literals are all in a union (int, double, string), but we only need to free them if they are strings
        if (node->type == TYPE_STRING) {
            free(node->as.literal.value.string_val);
        }
        break;
    case AST_IDENTIFIER:
        free(node->as.identifier.name);
        break;
    case AST_UNARY_OPERATOR:
        ast_free(node->as.unary.operand);
        break;
    case AST_BINARY_OPERATOR:
        ast_free(node->as.binary.left);
        ast_free(node->as.binary.right);
        break;
    case AST_CALL:
        free(node->as.call.name);
        ast_list_free(&node->as.call.args);
        free(node->as.call.arg_list_sizes);
        break;
    }

    free(node);
}
