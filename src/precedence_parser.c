/**
 * @file precedence_parser.c
 * @brief Precedence parser for IFJ26 expressions
 * @author David Zoblivý (xzoblid00)
 * @date 17.11.2026
 */

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

/**
 * @brief creates a dynamically allocated copy of a string
 * @param[in] text string to copy
 * @return pointer to the copy, or NULL if malloc fails
 */
static char *copy_string(const char *text) {
    size_t length = strlen(text) + 1;
    char *copy = malloc(length);
    if (copy == NULL) {
        return NULL;
    }

    strcpy(copy, text);
    return copy;
}

/**
 * @brief frees the stack, its AST nodes and owned token strings.
 * @param[in,out] stack to free.
 */
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

/**
 * @brief converts token type to precedence symbol
 * @param[in] type of token
 * @param[in] needs_operand when true unary +/-, else binary +/-
 * @param[out] symbol receive converted symbol on success
 * @return true if token can be converted, else false
 */
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

/**
 * @brief uses precedence table to get the right action
 * @param[in] stack_symbol y-axis of the precedence table
 * @param[in] input_symbol x-axis of the precedence table
 * @return corresponding action, or ACTION_ERROR if relation is invalid
 */
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

/**
 * @brief checks whether an item is a terminal with the specified symbol
 * @param[in] item to check
 * @param[in] symbol that is expected
 * @return true if both type and symbol match, otherwise false
 */
static bool is_terminal(const StackItem *item, PrecedenceSymbol symbol)
{
    return item->type == STACK_TERMINAL && item->symbol == symbol;
}

/**
 * @brief determine which reduction rule to choose
 * @param[in] stack to look into
 * @return according reduction type, or REDUCTION_INVALID if no rule matches
 */
static ReductionType get_reduction_type(const PrecedenceStack *stack)
{
    size_t boundary_index;

    if (stack_take_top_boundary(stack, &boundary_index) != true) {
        return REDUCTION_INVALID;
    }

    size_t rule_length = stack->length - boundary_index - 1;
    const StackItem *first = &stack->items[boundary_index + 1];

    switch (rule_length) {
        case 1:
            if (is_terminal(first, SYMBOL_OPERAND)) {
                return REDUCTION_OPERAND;
            }
            return REDUCTION_INVALID;

        case 2: {
            const StackItem *second = &stack->items[boundary_index + 2];

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
            const StackItem *second = &stack->items[boundary_index + 2];
            const StackItem *third = &stack->items[boundary_index + 3];

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

/**
 * @brief creates a literal or identifier AST node from a token
 * @param[in] token to create a node from
 * @param[out] result receives a new node, or NULL on failure
 * @return 0 on success, otherwise error code
 */
static int create_node_operand(const Token *token, AstNode **result) {
    *result = NULL;

    AstKind kind = AST_LITERAL;
    if (token->type == TOK_ID) {
        kind = AST_IDENTIFIER;
    }

    AstNode *node = ast_new(kind, token->line);
    if (node == NULL) {
        return 99;
    }

    switch (token->type) {
        case TOK_INT_LITERAL:
            node->type = TYPE_INT;
            node->as.literal.value.int_val = token->data.num_int_val; 
            break;

        case TOK_DOUBLE_LITERAL:
            node->type = TYPE_DOUBLE;
            node->as.literal.value.double_val = token->data.num_float_val;
            break;

        case TOK_TRUE:
        case TOK_FALSE:
            node->type = TYPE_BOOL;
            if (token->type == TOK_TRUE) {
                node->as.literal.value.int_val = 1;
            }
            else {
                node->as.literal.value.int_val = 0;
            }
            break;

        case TOK_STRING_LITERAL:
            node->type = TYPE_STRING;
            node->as.literal.value.string_val = copy_string(token->data.str_val);
            if (node->as.literal.value.string_val == NULL) {
                free(node);
                return 99;
            }
            break;

        case TOK_ID:
            node->type = TYPE_UNKNOWN;
            node->as.identifier.name = copy_string(token->data.str_val);
            if (node->as.identifier.name == NULL) {
                free(node);
                return 99;
            }
            break;

        default:
            ast_free(node);
            return 2;
    }

    *result = node;
    return 0;
}

/**
 * @brief applies one reduction rule from precedence table to the precedence stack
 * @param[in, out] stack to make a reduction from
 * @return 0 on success, otherwise error code
 */
static int reduce_expression(PrecedenceStack *stack) {
    ReductionType reduction = get_reduction_type(stack);
                
    if (reduction == REDUCTION_OPERAND) {
        size_t boundary_index;
        if (stack_take_top_boundary(stack, &boundary_index) != true) {
            return 99;
        }
        
        StackItem *operand = &stack->items[boundary_index + 1];

        int status = create_node_operand(&operand->token, &operand->ast);
        if (status != 0) {
            return status;
        }
        
        if (operand->token.type == TOK_ID || operand->token.type == TOK_STRING_LITERAL) {
            free(operand->token.data.str_val);
            operand->token.data.str_val = NULL;
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
        else if (stack->items[boundary_index + 2].token.type == TOK_AND) {
            node->as.binary.operator_type = OP_AND;
        }
        else if (stack->items[boundary_index + 2].token.type == TOK_OR) {
            node->as.binary.operator_type = OP_OR;
        }
        
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

/**
 * @brief parse a function call and create AST
 * @param[in, out] parser provider for the input tokens
 * @param[out] result receives the call node, or NULL on failure
 * @return 0 on success, otherwise a parser error code
 */
int parse_function_call(Parser *parser, AstNode **result) {
    *result = NULL;
    Token *token = &parser->current;
    AstNode *node = ast_new(AST_CALL, token->line);
    if (node == NULL) {
        return 99;
    }

    node->type = TYPE_UNKNOWN;
    node->as.call.name = copy_string(token->data.str_val);
    if (node->as.call.name == NULL) {
        ast_free(node);
        return 99;
    }

    ParseResult status = next_token(parser);
    if (status != PARSE_OK) {
        ast_free(node);
        return status;
    }

    status = next_token(parser);
    if (status != PARSE_OK) {
        ast_free(node);
        return status;
    }

    if (token->type == TOK_RIGHT_PARENTHESIS) {
        status = next_token(parser);
        if (status != PARSE_OK) {
            ast_free(node);
            return status;
        }

        *result = node;
        return 0;
    }

    while (1) {
        AstNode *argument = NULL;
        ParseResult status = parse_expression(parser, &argument);
        if (status != PARSE_OK) {
            ast_free(node);
            return status;
        }

        bool argument_added = ast_list_push(&node->as.call.args, argument);
        if (argument_added != true) {
            ast_free(node);
            ast_free(argument);
            return 99;
        }

        if (token->type == TOK_RIGHT_PARENTHESIS) {
            status = next_token(parser);
            if (status != PARSE_OK) {
                ast_free(node);
                return status;
            }
            
            *result = node;
            return 0;
        }

        if (token->type != TOK_COMMA) {
            ast_free(node);
            return 2;
        }

        status = next_token(parser);
        if (status != PARSE_OK) {
            ast_free(node);
            return status;
        }

        if (token->type == TOK_RIGHT_PARENTHESIS) {
            // line feed is required between a trailing comma and a right parenthesis
            if (token->lf_before == false) {
                ast_free(node);
                return 2;
            }

            status = next_token(parser);
            if (status != PARSE_OK) {
                ast_free(node);
                return status;
            }

            *result = node;
            return 0;
        }
    }
}

/**
 * @brief parse an interpolated string and create AST
 * @param[in, out] parser provider for the input tokens
 * @param[out] result receives the string AST, or NULL on failure
 * @return 0 on success, otherwise parser error code
 */
int parse_interpolated_string(Parser *parser, AstNode **result) {
    *result = NULL;
    Token *token = &parser->current;

    ParseResult status = next_token(parser);
    if (status != PARSE_OK) {
        return status;
    }

    // start with an empty string and append each interpolation part.
    AstNode *node = ast_new(AST_LITERAL, token->line);
    if (node == NULL) {
        return 99;
    }

    node->type = TYPE_STRING;
    node->as.literal.value.string_val = malloc(sizeof(char));
    if (node->as.literal.value.string_val == NULL) {
        ast_free(node);
        return 99;
    }
    // empty prefix handles s""
    node->as.literal.value.string_val[0] = '\0';

    while (token->type != TOK_INTERPOLATED_END) {
        AstNode *part = NULL;

        if (token->type == TOK_STRING_LITERAL || token->type == TOK_ID) {
            int status = create_node_operand(token, &part);
            if (status != 0) {
                ast_free(node);
                return status;
            }
        }
        else if (token->type == TOK_INTERPOLATION_BEGIN) {
            ParseResult status = next_token(parser);
            if (status != PARSE_OK) {
                ast_free(node);
                return status;
            }

            status = parse_expression(parser, &part);
            if (status != 0) {
                ast_free(node);
                ast_free(part);
                return status;
            }

            if (token->type != TOK_INTERPOLATION_END) {
                ast_free(node);
                ast_free(part);
                return 2;
            }
        }
        else {
            ast_free(part);
            ast_free(node);
            return 2;
        }

        // wrap inserted ${} or $id in toStr() to get a plain text
        if (token->type == TOK_ID || token->type == TOK_INTERPOLATION_END) {
            AstNode *conversion_to_string = ast_new(AST_CALL, part->line);
            if (conversion_to_string == NULL) {
                ast_free(part);
                ast_free(node);
                return 99;
            }

            conversion_to_string->type = TYPE_STRING;
            conversion_to_string->as.call.name = malloc(sizeof("toStr"));
            if (conversion_to_string->as.call.name == NULL) {
                ast_free(conversion_to_string);
                ast_free(node);
                ast_free(part);
                return 99;
            }
            strcpy(conversion_to_string->as.call.name, "toStr");

            if (ast_list_push(&conversion_to_string->as.call.args, part) != true) {
                ast_free(conversion_to_string);
                ast_free(node);
                ast_free(part);
                return 99;
            }

            part = conversion_to_string;
        }

        AstNode *connect = ast_new(AST_BINARY_OPERATOR, part->line);
        if (connect == NULL) {
            ast_free(node);
            ast_free(part);
            return 99;
        }

        connect->type = TYPE_STRING;
        connect->as.binary.operator_type = OP_ADD;
        connect->as.binary.left = node;
        connect->as.binary.right = part;
        node = connect;

        ParseResult status = next_token(parser);
        if (status != PARSE_OK) {
            ast_free(node);
            return status;
        }
    }

    status = next_token(parser);
    if (status != PARSE_OK) {
        ast_free(node);
        return status;
    }
    
    *result = node;
    return 0;
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

        // parse the interpolated string as one whole expression.
        if (token->type == TOK_INTERPOLATED_BEGIN && needs_operand == true) {
            AstNode *string = NULL;
            int result = parse_interpolated_string(parser, &string);
            if (result != 0) {
                stack_and_ast_free(&stack);
                return result;
            }

            StackItem item = {0};
            item.type = STACK_EXPRESSION;
            item.ast = string;
            if (stack_push(&stack, item) != true) {
                stack_and_ast_free(&stack);
                ast_free(string);
                return 99;
            }

            needs_operand = false;
            continue;
        }

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

        if (token->type == TOK_DOT) {
            if (needs_operand == true) {
                stack_and_ast_free(&stack);
                return 2;
            }

            if (stack.items[stack.length - 1].type != STACK_EXPRESSION) {
                int reduction_result = reduce_expression(&stack);

                if (reduction_result != 0) {
                    stack_and_ast_free(&stack);
                    return reduction_result;
                }
            }

            ParseResult next_result = next_token(parser);
            if (next_result != PARSE_OK) {
                stack_and_ast_free(&stack);
                return next_result;
            }

            if (token->type != TOK_ID) {
                stack_and_ast_free(&stack);
                return 2;
            }

            AstNode *node = ast_new(AST_CALL, token->line);
            if (node == NULL) {
                stack_and_ast_free(&stack);
                return 99;
            }
            
            node->type = TYPE_UNKNOWN;
            node->as.call.name = copy_string(token->data.str_val);
            if (node->as.call.name == NULL) {
                ast_free(node);
                stack_and_ast_free(&stack);
                return 99;
            }

            if (ast_list_push(&node->as.call.args, stack.items[stack.length - 1].ast) != true) {
                ast_free(node);
                stack_and_ast_free(&stack);
                return 99;
            }
            stack.items[stack.length - 1].ast = node;

            next_result = next_token(parser);
            if (next_result != PARSE_OK) {
                stack_and_ast_free(&stack);
                return next_result;
            }
            continue;
        }

        if (token->type == TOK_EOL || token->type == TOK_EOF || token->type == TOK_SEMICOLON) {
            input_symbol = SYMBOL_END;
        }
        else if (token->type == TOK_RIGHT_PARENTHESIS && parentheses_depth == 0) {
            input_symbol = SYMBOL_END;
        } 
        else if (token->type == TOK_COMMA && parentheses_depth == 0) {
            input_symbol = SYMBOL_END;
        }
        else if (token->type == TOK_INTERPOLATION_END && parentheses_depth == 0) {
            input_symbol = SYMBOL_END;
        }
        else if ((token->type == TOK_KW_DO || token->type == TOK_KW_THEN) && parentheses_depth == 0) {
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