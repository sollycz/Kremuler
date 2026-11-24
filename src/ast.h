/**
 * @file ast.h
 * @brief Abstract syntax tree of IFJ26
 * @author Oleksandr Teslja (xtesljo00)
 * @todo add some missing comments for the rest of the interface
 */

#ifndef AST_H
#define AST_H

#include "types.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum AstKind {
    /** overall program structure */
    AST_PROGRAM,
    AST_FUNCTION,
    AST_PARAMETER,
    AST_BLOCK,
    /** ((statements)) */
    AST_WHILE,
    AST_FOR, // CONTROL
    AST_IF,  // also used as an expression (CONTROL)
    AST_VARIABLE_DEFINITION,
    AST_ASSIGN,
    AST_RETURN,
    AST_EXPRESSION_STATEMENT,
    /** ((expressions)) */
    AST_LITERAL,
    AST_IDENTIFIER,
    AST_UNARY_OPERATOR,
    AST_BINARY_OPERATOR,
    AST_CALL,
} AstKind;

typedef enum AstOperatorType {
    OP_NEG, // unary -x
    OP_POS, // unary +x
    OP_NOT, // unary !x
    /** (binary) arithmetic operators */
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    /** (binary) relational operators */
    OP_LT, // <
    OP_LE, // <=
    OP_GT, // >
    OP_GE, // >=
    OP_EQ, // ==
    OP_NE, // !=
    /** (binary) logical operators */
    OP_AND, // &&
    OP_OR,  // ||
} AstOperatorType;

typedef struct AstNode AstNode;

// dynamic list of nodes
// (functions in the program, parameters in a function, statements inside of a block, arguments in a function call)
typedef struct AstNodeList {
    AstNode **items;
    size_t count;
} AstNodeList;

// AST node
struct AstNode {
    AstKind node_kind;
    unsigned int line;   // source line for error messages
    DataType type;       // TYPE_UNKNOWN if not yet determined
    unsigned int var_id; // unique id for variables, will be assigned by the semantic analyser

    union {
        struct {
            AstNodeList functions;
        } program;

        struct {
            char *name;
            AstNodeList parameters; // all parameters from all lists, flattened
            size_t *list_sizes;
            size_t list_count;
            DataType return_type; // also TYPE_UNKNOWN if not explicitly written
            AstNode *block;
            AstNode *receiver; // AST_PARAMETER of the extended value, NULL for a normal function (EXTKW)
            bool is_main_func;
        } function;

        struct {
            char *name;
            DataType type;
        } parameter;

        struct {
            AstNodeList statements;
        } block;

        struct {
            AstNode *condition;
            AstNode *block;
        } while_statement;

        struct {
            char *var_name;
            AstNode *from;
            AstNode *to;
            AstNode *step;      // NULL if "by" is missing
            bool inclusive;     // "to" = true, "until" = false
            AstNodeList guards; // conditions of "if" guards, may be empty
            AstNode *block;
        } for_statement;

        struct {
            AstNode *condition;
            AstNode *then_block;
            AstNode *else_block;
        } if_statement;

        struct {
            char *name;
            DataType declared_type; // TYPE_UNKNOWN if not written
            AstNode *value;
            bool is_mutable; // var = true, val = false
        } variable_definition;

        struct {
            char *name;
            AstNode *value;
        } assign;

        struct {
            AstNode *value; // will be NULL for an "empty" Unit () return
        } return_statement;

        struct {
            AstNode *expression;
        } expression_statement;

        struct {
            union {
                int64_t int_val;
                double double_val;
                char *string_val;
            } value;
        } literal;

        struct {
            char *name;
        } identifier;

        struct {
            AstOperatorType operator_type;
            AstNode *operand;
        } unary;

        struct {
            AstOperatorType operator_type;
            AstNode *left;
            AstNode *right;
        } binary;

        struct {
            char *name;
            AstNodeList args;
            size_t *arg_list_sizes;
            size_t arg_list_count;
            bool is_member;
        } call;
    } as;
};

AstNode *ast_new(AstKind kind, unsigned int line);
bool ast_list_push(AstNodeList *list, AstNode *node);
void ast_free(AstNode *node);

#endif
