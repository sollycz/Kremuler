#include "precedence_parser.h"
#include "precedence_stack.h"
#include "token.h"
#include "ast.h"

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

typedef enum {
    REDUCTION_INVALID,
    REDUCTION_OPERAND,
    REDUCTION_UNARY_OPERATION,
    REDUCTION_BINARY_OPERATION,
    REDUCTION_PARENTHESES,
    REDUCTION_UNIT
} ReductionType;

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

static ReductionType get_reduction_type(const PrecedenceStack *stack)
{
    size_t boundary;

    if (stack_take_top_boundary(stack, &boundary) != true) {
        return REDUCTION_INVALID;
    }

    size_t count = stack->length - boundary - 1;
    const StackItem *first = &stack->items[boundary + 1];

    switch (count) {
        case 1:
            if (is_terminal(first, SYMBOL_OPERAND)) {
                return REDUCTION_OPERAND;
            }
            return REDUCTION_INVALID;

        case 2: {
            const StackItem *second = &stack->items[boundary + 2];

            if (is_terminal(first, SYMBOL_UNARY_PLUS)  || 
                is_terminal(first, SYMBOL_UNARY_MINUS) || 
                is_terminal(first, SYMBOL_NOT)) {
                if (second->type == STACK_EXPRESSION) {
                    return REDUCTION_UNARY_OPERATION;
                }
                return REDUCTION_INVALID;
            }

            else if (is_terminal(first, SYMBOL_LEFT_PARENTHESIS) && is_terminal(second, SYMBOL_RIGHT_PARENTHESIS)) {
                return REDUCTION_UNIT;
            }

            return REDUCTION_INVALID;
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
                        return REDUCTION_BINARY_OPERATION;
                    }
                    return REDUCTION_INVALID;
                }
                return REDUCTION_INVALID;
            }

            if (is_terminal(first, SYMBOL_LEFT_PARENTHESIS)) {
                if (second->type == STACK_EXPRESSION) {
                    if (is_terminal(third, SYMBOL_RIGHT_PARENTHESIS) == true) {
                        return REDUCTION_PARENTHESES;
                    }
                }
                return REDUCTION_INVALID;
            }

            return REDUCTION_INVALID;
        }

        default:
            return REDUCTION_INVALID;
    }
}

static int reduce_expression(PrecedenceStack *stack) {
    ReductionType reduction = get_reduction_type(stack);
                
    if (reduction == REDUCTION_OPERAND) {
        size_t boundary_index;
        if (stack_take_top_boundary(stack, &boundary_index) != true) {
            return 99;
        }
        
        if (stack->items[boundary_index + 1].token.type == TOK_INT_LITERAL) {
            AstNode *node = ast_new(AST_LITERAL, stack->items[boundary_index + 1].token.line);
            if (node == NULL) {
                return 99;
            }

            node->type = TYPE_INT;
            node->as.literal.value.int_val = stack->items[boundary_index + 1].token.data.num_int_val;
            stack->items[boundary_index + 1].ast = node;
        }

        if (stack->items[boundary_index + 1].token.type == TOK_DOUBLE_LITERAL) {
            AstNode *node = ast_new(AST_LITERAL, stack->items[boundary_index + 1].token.line);
            if (node == NULL) {
                return 99;
            }

            node->type = TYPE_DOUBLE;
            node->as.literal.value.double_val = stack->items[boundary_index + 1].token.data.num_float_val;
            stack->items[boundary_index + 1].ast = node;
        }

        if (stack->items[boundary_index + 1].token.type == TOK_TRUE || stack->items[boundary_index + 1].token.type == TOK_FALSE) {
            AstNode *node = ast_new(AST_LITERAL, stack->items[boundary_index + 1].token.line);
            if (node == NULL) {
                return 99;
            }

            node->type = TYPE_BOOL;
            if (stack->items[boundary_index + 1].token.type == TOK_TRUE) { 
                node->as.literal.value.int_val = 1; // TODO: PŘEDĚLAT NA .BOOL_VAL AŽ BUDE!
            }
            else {
                node->as.literal.value.int_val = 0; // TODO: PŘEDĚLAT NA .BOOL_VAL AŽ BUDE!
            }
            stack->items[boundary_index + 1].ast = node;
        }

        stack->items[boundary_index + 1].type = STACK_EXPRESSION;
        stack->items[boundary_index] = stack->items[boundary_index + 1];

        stack->length--;
        return 0;
    }
    else if (reduction == REDUCTION_UNIT) {
        size_t boundary_index;
        if (stack_take_top_boundary(stack, &boundary_index) != true) {
            return 99;
        }

        AstNode *node = ast_new(AST_LITERAL, stack->items[boundary_index + 1].token.line);
        if (node == NULL) {
            return 99;
        }

        node->type = TYPE_UNIT;
        stack->items[boundary_index].type = STACK_EXPRESSION;
        stack->items[boundary_index].ast = node;

        stack->length -= 2;
        return 0;
    } 
    else if (reduction == REDUCTION_PARENTHESES) {
        size_t boundary_index;
        if (stack_take_top_boundary(stack, &boundary_index) != true) {
            return 99;
        }

        stack->items[boundary_index] = stack->items[boundary_index + 2];

        stack->length -= 3;
        return 0;
    }
    else if (reduction == REDUCTION_UNARY_OPERATION) { 
        size_t boundary_index;
        if (stack_take_top_boundary(stack, &boundary_index) != true) {
            return 99;
        }

        stack->items[boundary_index] = stack->items[boundary_index + 2]; 

        stack->length -= 2;
        return 0;
    }
    else if (reduction == REDUCTION_INVALID) {
        return 2;
    }
    else if (reduction == REDUCTION_BINARY_OPERATION) {
        size_t boundary_index;
        if (stack_take_top_boundary(stack, &boundary_index) != true) {
            return 99;
        }

        stack->items[boundary_index] = stack->items[boundary_index + 1]; 

        stack->length -=3;
        return 0;
    }
    return 2;
}

static PrecedenceAction get_action(PrecedenceSymbol stack_symbol, PrecedenceSymbol input_symbol) {
    switch(precedence_table[stack_symbol][input_symbol]) {
        case '<':
            return ACTION_BEGIN_AND_PUSH;
        case '=':
            return ACTION_PUSH;
        case '>':
        return ACTION_REDUCE;
        default:
        return ACTION_ERROR;
    }
}

int parse_expression(Token *token) {
    PrecedenceStack stack;
    if (stack_init(&stack) != true) {
        return 99;
    }

    StackItem end = {0};
    end.type = STACK_TERMINAL;
    end.symbol = SYMBOL_END;
    if (stack_push(&stack, end) != true) {
        stack_free(&stack);
        return 99;
    }

    bool needs_operand = true;
    PrecedenceSymbol input_symbol;

    while (1) {

        if (token->type == TOK_EOL || token->type == TOK_EOF) {
            input_symbol = SYMBOL_END;
        }
        else if (token_to_symbol(token->type, needs_operand, &input_symbol) != true) {
            stack_free(&stack);
            return 2;
        }

        size_t terminal_index;
        if (stack_take_top_terminal(&stack, &terminal_index) != true) {
            stack_free(&stack);
            return 99;
        }

        if (input_symbol == SYMBOL_END && stack.length == 2 &&
            is_terminal(&stack.items[0], SYMBOL_END) && 
            stack.items[1].type == STACK_EXPRESSION) {
                stack_free(&stack);
                return 0;
        }

        PrecedenceAction action = get_action(stack.items[terminal_index].symbol, input_symbol);

        switch (action) {
            case ACTION_BEGIN_AND_PUSH: {
                StackItem item = {0};
                item.type = STACK_TERMINAL;
                item.symbol = input_symbol;
                item.token = *token;

                StackItem boundary = {0};
                boundary.type = STACK_BOUNDARY;
                if (stack_insert(&stack, terminal_index + 1, boundary) != true) {
                    stack_free(&stack);
                    return 99;
                }

                if (stack_push(&stack, item) != true) {
                    stack_free(&stack);
                    return 99;
                }

                if (input_symbol == SYMBOL_OPERAND ||
                    input_symbol == SYMBOL_RIGHT_PARENTHESIS) {
                    needs_operand = false;
                } 
                else {
                    needs_operand = true;
                }
                
                break;
            }
            case ACTION_PUSH: {
                StackItem item = {0};
                item.type = STACK_TERMINAL;
                item.symbol = input_symbol;
                item.token = *token;

                if (stack_push(&stack, item) != true) {
                    stack_free(&stack);
                    return 99;
                }

                if (input_symbol == SYMBOL_OPERAND ||
                    input_symbol == SYMBOL_RIGHT_PARENTHESIS) {
                    needs_operand = false;
                } 
                else {
                    needs_operand = true;
                }

                break;
            }
            case ACTION_REDUCE: {
                int result = reduce_expression(&stack);
                if (result != 0) {
                    stack_free(&stack);
                    return result;
                } 
                break;
            }

            case ACTION_ERROR:
                stack_free(&stack);
                return 2;

            default:
                stack_free(&stack);
                return 99;
        }
    }
}