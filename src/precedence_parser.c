#include "precedence_parser.h"
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
