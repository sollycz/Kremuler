#ifndef PRECEDENCE_STACK_H
#define PRECEDENCE_STACK_H

#include <stddef.h>
#include <stdbool.h>
#include "token.h"
#include "precedence_parser.h"
#include "ast.h"

typedef enum {
    STACK_TERMINAL,
    STACK_EXPRESSION,
    STACK_BOUNDARY
} StackItemType;

typedef struct {
    StackItemType type;
    PrecedenceSymbol symbol;
    Token token;
    AstNode *ast;
} StackItem;

typedef struct {
    StackItem *items;
    size_t length;
    size_t capacity;
} PrecedenceStack;

bool stack_init(PrecedenceStack *stack);
void stack_free(PrecedenceStack *stack);
bool stack_push(PrecedenceStack *stack, StackItem item);
bool stack_pop(PrecedenceStack *stack, StackItem *item);
bool stack_take_top_terminal(const PrecedenceStack *stack, size_t *index);
bool stack_take_top_boundary(const PrecedenceStack *stack, size_t *index);
bool stack_insert(PrecedenceStack *stack, size_t index, StackItem item);

#endif