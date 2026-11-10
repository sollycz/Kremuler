#include "precedence_stack.h"
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#define STACK_INIT_CAPACITY 16

bool stack_init(PrecedenceStack *stack) 
{
    stack->length = 0;
    stack->capacity = 0;

    stack->items = malloc(STACK_INIT_CAPACITY * sizeof(StackItem));
    if (stack->items == NULL) {
        return false;
    }

    stack->capacity = STACK_INIT_CAPACITY;
    return true;
}

void stack_free(PrecedenceStack *stack) 
{
    free(stack->items);
    stack->items = NULL;
    stack->capacity = 0;
    stack->length = 0;
}

static bool stack_increase(PrecedenceStack *stack)
{
    if (stack->capacity > SIZE_MAX / 2 / sizeof(StackItem)) {
        return false;
    }

    size_t new_capacity = stack->capacity * 2;

    StackItem *new_items = realloc(stack->items, new_capacity * sizeof(StackItem));
    if (new_items == NULL) {
        return false;
    }

    stack->items = new_items;
    stack->capacity = new_capacity;

    return true;
}

bool stack_push(PrecedenceStack *stack, StackItem item) 
{
    if (stack->length == stack->capacity) {
        bool result = stack_increase(stack);
        if (result != true) {
            return false;
        }
    }

    stack->items[stack->length] = item;
    stack->length++;

    return true;
}

bool stack_pop(PrecedenceStack *stack, StackItem *item)
{
    if (stack->length == 0) {
        return false;
    }
    stack->length--;

    *item = stack->items[stack->length];

    return true;
}

bool stack_take_top_terminal(const PrecedenceStack *stack, size_t *index) 
{
    size_t tmp = stack->length;

    while (tmp > 0) {
        tmp--;
        if (stack->items[tmp].type == STACK_TERMINAL) {
            *index = tmp;
            return true;
        }
    }

    return false;
}

bool stack_take_top_boundary(const PrecedenceStack *stack, size_t *index) 
{
    size_t tmp = stack->length;

    while (tmp > 0) {
        tmp--;
        if (stack->items[tmp].type == STACK_BOUNDARY) {
            *index = tmp;
            return true;
        }
    }

    return false;
}

bool stack_insert(PrecedenceStack *stack, size_t index, StackItem item) 
{
    if (index > stack->length) {
        return false;
    }

    if (stack->length == stack->capacity) {
        bool result = stack_increase(stack);
        if (result != true) {
            return false;
        }
    }

    size_t tmp = stack->length;
    while(tmp > index) {
        stack->items[tmp] = stack->items[tmp - 1];
        tmp--;
    }
    stack->items[tmp] = item;
    stack->length++;
    
    return true;
}