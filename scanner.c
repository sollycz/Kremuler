#include <stdio.h>
#include "scanner.h"

typedef enum {
    STATE_START,
    STATE_PLUS,
    STATE_MINUS,
    STATE_SLASH,
    STATE_MULTIPLY,
    STATE_LESS,
    STATE_LESS_OR_EQUAL,
    STATE_GREATER,
    STATE_GREATER_OR_EQUAL,
    STATE_LEFT_PARENTHESES,
    STATE_RIGHT_PARENTHESES,
    STATE_COMMA,
    STATE_COLON,
    STATE_SEMICOLON,
    STATE_AT,
    STATE_QUOTATION_MARK,
    STATE_AMPERSAND,
    STATE_VERTICAL_BAR,
    STATE_DOT
} State;

int scanner_get_token(Token *token) {
    int c = getchar();
    State state = STATE_START;

    while (1) {
        switch (state) {
            case STATE_START:
                if (c == EOF) {
                    token->type = TOK_EOF;
                    return 0;
                }

                return -1;

            default:
                return 42; 
        }
    }
}
