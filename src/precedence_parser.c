#include "precedence_parser.h"
#include "precedence_stack.h"
#include "token.h"

static const char precedence_table[19][19] = {
    {'>', '>', '<', '<', '>', '>', '>', '>', '>', '>', '<', '<', '<', '>', '>', '<', '>', '<', '>'}, // +
    {'>', '>', '<', '<', '>', '>', '>', '>', '>', '>', '<', '<', '<', '>', '>', '<', '>', '<', '>'}, // -
    {'>', '>', '>', '>', '>', '>', '>', '>', '>', '>', '<', '<', '<', '>', '>', '<', '>', '<', '>'}, // *
    {'>', '>', '>', '>', '>', '>', '>', '>', '>', '>', '<', '<', '<', '>', '>', '<', '>', '<', '>'}, // /
    {'<', '<', '<', '<', ' ', ' ', ' ', ' ', '>', '>', '<', '<', '<', '>', '>', '<', '>', '<', '>'}, // <
    {'<', '<', '<', '<', ' ', ' ', ' ', ' ', '>', '>', '<', '<', '<', '>', '>', '<', '>', '<', '>'}, // <=
    {'<', '<', '<', '<', ' ', ' ', ' ', ' ', '>', '>', '<', '<', '<', '>', '>', '<', '>', '<', '>'}, // >
    {'<', '<', '<', '<', ' ', ' ', ' ', ' ', '>', '>', '<', '<', '<', '>', '>', '<', '>', '<', '>'}, // >=
    {'<', '<', '<', '<', '<', '<', '<', '<', ' ', ' ', '<', '<', '<', '>', '>', '<', '>', '<', '>'}, // ==
    {'<', '<', '<', '<', '<', '<', '<', '<', ' ', ' ', '<', '<', '<', '>', '>', '<', '>', '<', '>'}, // !=
    {'>', '>', '>', '>', '>', '>', '>', '>', '>', '>', '<', '<', '<', '>', '>', '<', '>', '<', '>'}, // u+
    {'>', '>', '>', '>', '>', '>', '>', '>', '>', '>', '<', '<', '<', '>', '>', '<', '>', '<', '>'}, // u-
    {'>', '>', '>', '>', '>', '>', '>', '>', '>', '>', '<', '<', '<', '>', '>', '<', '>', '<', '>'}, // !
    {'<', '<', '<', '<', '<', '<', '<', '<', '<', '<', '<', '<', '<', '>', '>', '<', '>', '<', '>'}, // &&
    {'<', '<', '<', '<', '<', '<', '<', '<', '<', '<', '<', '<', '<', '<', '>', '<', '>', '<', '>'}, // ||
    {'<', '<', '<', '<', '<', '<', '<', '<', '<', '<', '<', '<', '<', '<', '<', '<', '=', '<', ' '}, // (
    {'>', '>', '>', '>', '>', '>', '>', '>', '>', '>', '>', '>', '>', '>', '>', ' ', '>', ' ', '>'}, // )
    {'>', '>', '>', '>', '>', '>', '>', '>', '>', '>', '>', '>', '>', '>', '>', ' ', '>', ' ', '>'}, // i
    {'<', '<', '<', '<', '<', '<', '<', '<', '<', '<', '<', '<', '<', '<', '<', '<', ' ', '<', ' '}, // $
};

static bool token_to_symbol(TokenType type, bool needs_operand, PrecedenceSymbol *symbol) {
    switch (type) {
        case TOK_PLUS:
            if (needs_operand == true) {
                *symbol = SYMBOL_UNARY_PLUS;
                return true;
            }
            else {
                *symbol = SYMBOL_PLUS;
                return true;
            }

        case TOK_MINUS:
            if (needs_operand == true) {
                *symbol = SYMBOL_UNARY_MINUS;
                return true;
            }
            else {
                *symbol = SYMBOL_MINUS;
                return true;
            }

        case TOK_MULTIPLY:
            *symbol = SYMBOL_MULTIPLY;
            return true;

        case TOK_DIVISION:
            *symbol = SYMBOL_DIVISION;
            return true;

        case TOK_LESS_THAN:
            *symbol = SYMBOL_LESS_THAN;
            return true;

        case TOK_LESS_OR_EQUAL_THAN:
            *symbol = SYMBOL_LESS_OR_EQUAL_THAN;
            return true;

        case TOK_GREATER_THAN:
            *symbol = SYMBOL_GREATER_THAN;
            return true;

        case TOK_GREATER_OR_EQUAL_THAN:
            *symbol = SYMBOL_GREATER_OR_EQUAL_THAN;
            return true;

        case TOK_EQUAL:
            *symbol = SYMBOL_EQUAL;
            return true;

        case TOK_NOT_EQUAL:
            *symbol = SYMBOL_NOT_EQUAL;
            return true;

        case TOK_NOT:
            *symbol = SYMBOL_NOT;
            return true;

        case TOK_AND:
            *symbol = SYMBOL_AND;
            return true;

        case TOK_OR:
            *symbol = SYMBOL_OR;
            return true;

        case TOK_LEFT_PARENTHESIS:
            *symbol = SYMBOL_LEFT_PARENTHESIS;
            return true;

        case TOK_RIGHT_PARENTHESIS:
            *symbol = SYMBOL_RIGHT_PARENTHESIS;
            return true;

        case TOK_ID:
        case TOK_INT_LITERAL:
        case TOK_DOUBLE_LITERAL:
        case TOK_STRING_LITERAL:
        case TOK_TRUE:
        case TOK_FALSE:
            *symbol = SYMBOL_OPERAND;
            return true;

        case TOK_EOF:
            *symbol = SYMBOL_END;
            return true;

        default:
            return false;
    }
}

static bool is_terminal(const StackItem *item, PrecedenceSymbol symbol)
{
    return item->type == STACK_TERMINAL && item->symbol == symbol;
}

static bool is_valid_reduction(const PrecedenceStack *stack)
{
    size_t boundary;

    if (stack_take_top_boundary(stack, &boundary) != true) {
        return false;
    }

    size_t count = stack->length - boundary - 1;
    const StackItem *first = &stack->items[boundary + 1];

    switch (count) {
        case 1:
            return (is_terminal(first, SYMBOL_OPERAND));

        case 2: {
            const StackItem *second = &stack->items[boundary + 2];

            if (is_terminal(first, SYMBOL_UNARY_PLUS)  || 
                is_terminal(first, SYMBOL_UNARY_MINUS) || 
                is_terminal(first, SYMBOL_NOT)) {
                if (second->type == STACK_EXPRESSION) {
                    return true;
                }
                return false;
            }

            else if (is_terminal(first, SYMBOL_LEFT_PARENTHESIS) && is_terminal(second, SYMBOL_RIGHT_PARENTHESIS)) {
                return true;
            }

            return false;
        }

        case 3: {
            const StackItem *second = &stack->items[boundary + 2];
            const StackItem *third = &stack->items[boundary + 3];

            if (first->type == STACK_EXPRESSION) {
                if (second->type == STACK_TERMINAL && (
                    is_terminal(second, SYMBOL_PLUS) ||
                    is_terminal(second, SYMBOL_MINUS) ||
                    is_terminal(second, SYMBOL_MULTIPLY) ||
                    is_terminal(second, SYMBOL_DIVISION) ||
                    is_terminal(second, SYMBOL_LESS_THAN) ||
                    is_terminal(second, SYMBOL_LESS_OR_EQUAL_THAN) ||
                    is_terminal(second, SYMBOL_GREATER_THAN) ||
                    is_terminal(second, SYMBOL_GREATER_OR_EQUAL_THAN) ||
                    is_terminal(second, SYMBOL_EQUAL) ||
                    is_terminal(second, SYMBOL_NOT_EQUAL) ||
                    is_terminal(second, SYMBOL_AND) ||
                    is_terminal(second, SYMBOL_OR))) {

                    if (third->type == STACK_EXPRESSION) {
                        return true;
                    }
                    return false;
                }
                return false;
            }

            if (is_terminal(first, SYMBOL_LEFT_PARENTHESIS)) {
                if (second->type == STACK_EXPRESSION) {
                    return is_terminal(third, SYMBOL_RIGHT_PARENTHESIS);
                }
                return false;
            }

            return false;
        }

        default:
            return false;
    }
}
