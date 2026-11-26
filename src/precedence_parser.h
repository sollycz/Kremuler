/**
 * @file precedence_parser.h
 * @brief Header file for the precedence parser for IFJ26
 * @author David Zoblivý (xzoblid00)
 * @date 17.11.2026
 */

#ifndef PRECEDENCE_PARSER_H
#define PRECEDENCE_PARSER_H
#include "token.h"
#include "ast.h"
#include "parser.h"

typedef enum {
    SYMBOL_PLUS,
    SYMBOL_MINUS,
    SYMBOL_MULTIPLY,
    SYMBOL_DIVISION,
    SYMBOL_LESS_THAN,
    SYMBOL_LESS_OR_EQUAL_THAN,
    SYMBOL_GREATER_THAN,
    SYMBOL_GREATER_OR_EQUAL_THAN,
    SYMBOL_EQUAL,
    SYMBOL_NOT_EQUAL,
    SYMBOL_UNARY_PLUS,
    SYMBOL_UNARY_MINUS,
    SYMBOL_NOT,
    SYMBOL_AND,
    SYMBOL_OR,
    SYMBOL_LEFT_PARENTHESIS,
    SYMBOL_RIGHT_PARENTHESIS,
    SYMBOL_OPERAND,
    SYMBOL_END,
} PrecedenceSymbol;

typedef enum {
    ACTION_BEGIN_AND_PUSH,
    ACTION_PUSH,
    ACTION_REDUCE,
    ACTION_ERROR
} PrecedenceAction;

/**
 * @brief parse an expression and create its AST
 * @param[in, out] parser positioned at the first expression token
 * @param[out] result expression AST, or NULL on failure
 * @return PARSE_OK on success, otherwise parser error code
 * @note The expression terminator is left unconsumed.
 */
ParseResult parse_expression(Parser *parser, AstNode **result);

#endif
