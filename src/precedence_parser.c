#include "precedence_parser.h"
#include "precedence_stack.h"
#include "token.h"
#include "ast.h"
#include "parser.h"
#include <string.h>
#include <stdlib.h>


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

static void stack_and_ast_free(PrecedenceStack *stack) {
    for (size_t i = 0; i < stack->length; i++) {
        if (stack->items[i].type == STACK_EXPRESSION) {
            ast_free(stack->items[i].ast);
        }
        else if ((stack->items[i].type == STACK_TERMINAL) && (stack->items[i].token.type == TOK_ID || stack->items[i].token.type == TOK_STRING_LITERAL)) {
            free(stack->items[i].token.data.str_val);
            stack->items[i].token.data.str_val = NULL;
        }
    }
    stack_free(stack);
}

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

        if (stack->items[boundary_index + 1].token.type == TOK_STRING_LITERAL) {
            AstNode *node = ast_new(AST_LITERAL, stack->items[boundary_index + 1].token.line);
            if (node == NULL) {
                return 99;
            }

            node->type = TYPE_STRING;

            size_t length = strlen(stack->items[boundary_index + 1].token.data.str_val) + 1;
            node->as.literal.value.string_val = malloc(length);
            if (node->as.literal.value.string_val == NULL) {
                free(node);
                return 99;
            }

            strcpy(node->as.literal.value.string_val, stack->items[boundary_index + 1].token.data.str_val);
            stack->items[boundary_index + 1].ast = node;

            free(stack->items[boundary_index + 1].token.data.str_val);
            stack->items[boundary_index + 1].token.data.str_val = NULL;
        }

        if (stack->items[boundary_index + 1].token.type == TOK_ID) {
            AstNode *node = ast_new(AST_IDENTIFIER, stack->items[boundary_index + 1].token.line);
            if (node == NULL) {
                return 99;
            }

            node->type = TYPE_UNKNOWN;

            size_t length = strlen(stack->items[boundary_index + 1].token.data.str_val) + 1;
            node->as.identifier.name = malloc(length);
            if (node->as.identifier.name == NULL) {
                free(node);
                return 99;
            }

            strcpy(node->as.identifier.name, stack->items[boundary_index + 1].token.data.str_val);
            stack->items[boundary_index + 1].ast = node;

            free(stack->items[boundary_index + 1].token.data.str_val);
            stack->items[boundary_index + 1].token.data.str_val = NULL;
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

        if (stack->items[boundary_index + 1].token.type == TOK_MINUS || stack->items[boundary_index + 1].token.type == TOK_PLUS) {
            AstNode *node = ast_new(AST_UNARY_OPERATOR, stack->items[boundary_index + 1].token.line);
            if (node == NULL) {
                return 99;
            }

            node->type = TYPE_UNKNOWN;
            if (stack->items[boundary_index + 1].token.type == TOK_MINUS) {
                node->as.unary.operator_type = OP_NEG;
            } else {
                node->as.unary.operator_type = OP_POS;
            }
            node->as.unary.operand = stack->items[boundary_index + 2].ast;
            stack->items[boundary_index] = stack->items[boundary_index + 2];
            stack->items[boundary_index].ast = node;

        }

        /*      UNCOMMENT AFTER ADDING OP_NOT TO AST.H
        if (stack->items[boundary_index + 1].token.type == TOK_NOT) {
            AstNode *node = ast_new(AST_UNARY_OPERATOR, stack->items[boundary_index + 1].token.line);
            if (node == NULL) {
                return 99;
                }

                node->type = TYPE_UNKNOWN;
                node->as.unary.operator_type = OP_NOT;
                node->as.unary.operand = stack->items[boundary_index + 2].ast;
                stack->items[boundary_index] = stack->items[boundary_index + 2];
                stack->items[boundary_index].ast = node;

            } */

        if (stack->items[boundary_index + 1].token.type == TOK_NOT) {
            return 2;
        }

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

        AstNode *node = ast_new(AST_BINARY_OPERATOR, stack->items[boundary_index + 2].token.line);
        if (node == NULL) {
            return 99;
        }

        node->type = TYPE_UNKNOWN;
        if (stack->items[boundary_index + 2].token.type == TOK_PLUS) {
            node->as.binary.operator_type = OP_ADD;
        } 
        else if (stack->items[boundary_index + 2].token.type == TOK_MINUS) {
            node->as.binary.operator_type = OP_SUB;
        }
        else if (stack->items[boundary_index + 2].token.type == TOK_MULTIPLY) {
            node->as.binary.operator_type = OP_MUL;
        }
        else if (stack->items[boundary_index + 2].token.type == TOK_DIVISION) {
            node->as.binary.operator_type = OP_DIV;
        }
        else if (stack->items[boundary_index + 2].token.type == TOK_LESS_THAN) {
            node->as.binary.operator_type = OP_LT;
        }
        else if (stack->items[boundary_index + 2].token.type == TOK_LESS_OR_EQUAL_THAN) {
            node->as.binary.operator_type = OP_LE;
        }
        else if (stack->items[boundary_index + 2].token.type == TOK_GREATER_THAN) {
            node->as.binary.operator_type = OP_GT;
        }
        else if (stack->items[boundary_index + 2].token.type == TOK_GREATER_OR_EQUAL_THAN) {
            node->as.binary.operator_type = OP_GE;
        }
        else if (stack->items[boundary_index + 2].token.type == TOK_EQUAL) {
            node->as.binary.operator_type = OP_EQ;
        }
        else if (stack->items[boundary_index + 2].token.type == TOK_NOT_EQUAL) {
            node->as.binary.operator_type = OP_NE;
        }
    /*   UNCOMMENT AFTER ADDING && AND || INTO AST.H
        else if (stack->items[boundary_index + 2].token.type == TOK_AND) {
            node->as.binary.operator_type = OP_AND;
        }
        else if (stack->items[boundary_index + 2].token.type == TOK_OR) {
            node->as.binary.operator_type = OP_OR;
        }
    */
        
        else {
            free(node);
            return 2;
        }

        node->as.binary.left = stack->items[boundary_index + 1].ast;
        node->as.binary.right = stack->items[boundary_index + 3].ast;
        stack->items[boundary_index] = stack->items[boundary_index + 1]; 
        stack->items[boundary_index].ast = node;

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

int parse_function_call(Parser *parser, AstNode **result) {
    *result = NULL;
    Token *token = &parser->current;
    AstNode *node = ast_new(AST_CALL, token->line);
    if (node == NULL) {
        return 99;
    }

    node->type = TYPE_UNKNOWN;
    size_t length = strlen(token->data.str_val) + 1;

    node->as.call.name = malloc(length);
    if (node->as.call.name == NULL) {
        free(node);
        return 99;
    }

    strcpy(node->as.call.name, token->data.str_val);

    ParseResult result_next_token = next_token(parser);
    if (result_next_token != PARSE_OK) {
        ast_free(node);
        return result_next_token;
    }

    result_next_token = next_token(parser);
    if (result_next_token != PARSE_OK) {
        ast_free(node);
        return result_next_token;
    }

    if (token->type == TOK_RIGHT_PARENTHESIS) {
        result_next_token = next_token(parser);
        if (result_next_token != PARSE_OK) {
            ast_free(node);
            return result_next_token;
        }

        *result = node;
        return 0;
    }

    while (1) {
        AstNode *argument = NULL;
        ParseResult result_parse_expression = parse_expression(parser, &argument);
        if (result_parse_expression != PARSE_OK) {
            ast_free(node);
            return result_parse_expression;
        }

        bool result_ast_list_push = ast_list_push(&node->as.call.args, argument);
        if (result_ast_list_push != true) {
            ast_free(node);
            ast_free(argument);
            return 99;
        }

        if (token->type == TOK_RIGHT_PARENTHESIS) {
            result_next_token = next_token(parser);
            if (result_next_token != PARSE_OK) {
                ast_free(node);
                return result_next_token;
            }
            
            *result = node;
            return 0;
        }

        if (token->type != TOK_COMMA) {
            ast_free(node);
            return 2;
        }

        result_next_token = next_token(parser);
        if (result_next_token != PARSE_OK) {
            ast_free(node);
            return result_next_token;
        }

        if (token->type == TOK_RIGHT_PARENTHESIS) {
            if (token->lf_before == false) {
                ast_free(node);
                return 2;
            }

            result_next_token = next_token(parser);
            if (result_next_token != PARSE_OK) {
                ast_free(node);
                return result_next_token;
            }

            *result = node;
            return 0;
        }
    }
}

int parse_expression(Parser *parser, AstNode **result) {
    *result = NULL;
    Token *token = &parser->current;
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

    size_t parentheses_depth = 0;

    while (1) {

        TokenType lookahead;
        if (token->type == TOK_ID && needs_operand == true) {
            ParseResult result = lookahead_token(parser, &lookahead);
            if (result != PARSE_OK) {
                stack_and_ast_free(&stack);
                return result;
            }
            if (lookahead == TOK_LEFT_PARENTHESIS) {
                AstNode *call = NULL;
                int result = parse_function_call(parser, &call);
                if (result != 0) {
                    stack_and_ast_free(&stack);
                    return result;
                }

                StackItem item = {0};
                item.type = STACK_EXPRESSION;
                item.ast = call;

                if (stack_push(&stack, item) != true) {
                    stack_and_ast_free(&stack);
                    ast_free(call);
                    return 99;
                }

                needs_operand = false;
                continue;
            }
        }

        if (token->type == TOK_EOL || token->type == TOK_EOF) {
            input_symbol = SYMBOL_END;
        }
        else if (token->type == TOK_RIGHT_PARENTHESIS && parentheses_depth == 0) {
            input_symbol = SYMBOL_END;
        } 
        else if (token->type == TOK_COMMA && parentheses_depth == 0) {
            input_symbol = SYMBOL_END;
        }
        else if (token_to_symbol(token->type, needs_operand, &input_symbol) != true) {
            stack_and_ast_free(&stack);
            return 2;
        }

        size_t terminal_index;
        if (stack_take_top_terminal(&stack, &terminal_index) != true) {
            stack_and_ast_free(&stack);
            return 99;
        }

        if (input_symbol == SYMBOL_END && stack.length == 2 &&
            is_terminal(&stack.items[0], SYMBOL_END) && 
            stack.items[1].type == STACK_EXPRESSION) {
                *result = stack.items[1].ast;
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
                    stack_and_ast_free(&stack);
                    return 99;
                }

                if (stack_push(&stack, item) != true) {
                    stack_and_ast_free(&stack);
                    return 99;
                }
                
                if (token->type == TOK_STRING_LITERAL || token->type == TOK_ID) {
                    token->data.str_val = NULL;
                }

                if (input_symbol == SYMBOL_OPERAND ||
                    input_symbol == SYMBOL_RIGHT_PARENTHESIS) {
                    needs_operand = false;
                } 
                else {
                    needs_operand = true;
                }

                if (input_symbol == SYMBOL_LEFT_PARENTHESIS) {
                    parentheses_depth++;
                }
                else if (input_symbol == SYMBOL_RIGHT_PARENTHESIS) {
                    if (parentheses_depth > 0) {
                        parentheses_depth--;
                    }
                }
                
                ParseResult next_result = next_token(parser);
                if (next_result != PARSE_OK) {
                    stack_and_ast_free(&stack);
                    return next_result;
                }
                break;
            }
            case ACTION_PUSH: {
                StackItem item = {0};
                item.type = STACK_TERMINAL;
                item.symbol = input_symbol;
                item.token = *token;

                if (stack_push(&stack, item) != true) {
                    stack_and_ast_free(&stack);
                    return 99;
                }

                if (token->type == TOK_STRING_LITERAL || token->type == TOK_ID) {
                    token->data.str_val = NULL;
                }

                if (input_symbol == SYMBOL_OPERAND ||
                    input_symbol == SYMBOL_RIGHT_PARENTHESIS) {
                    needs_operand = false;
                } 
                else {
                    needs_operand = true;
                }

                if (input_symbol == SYMBOL_LEFT_PARENTHESIS) {
                    parentheses_depth++;
                }
                else if (input_symbol == SYMBOL_RIGHT_PARENTHESIS) {
                    if (parentheses_depth > 0) {
                        parentheses_depth--;
                    }
                }

                ParseResult next_result = next_token(parser);
                if (next_result != PARSE_OK) {
                    stack_and_ast_free(&stack);
                    return next_result;
                }
                break;
            }
            case ACTION_REDUCE: {
                int result = reduce_expression(&stack);
                if (result != 0) {
                    stack_and_ast_free(&stack);
                    return result;
                } 
                break;
            }

            case ACTION_ERROR:
                stack_and_ast_free(&stack);
                return 2;

            default:
                stack_and_ast_free(&stack);
                return 99;
        }
    }
}