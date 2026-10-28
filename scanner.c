#include <stdio.h>
#include <stdlib.h>
#include "scanner.h"
#include "scanner_buffer.h"

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
    STATE_DOT,
    STATE_INT_ZERO,
    STATE_INT_DIGITS,
    STATE_INT_UNDERSCORE,
    STATE_INT_DIGITS_AFTER_ZERO,
    STATE_INT_UNDERSCORE_AFTER_ZERO
} State;

int scanner_get_token(Token *token) {
    int c = getchar();
    State state = STATE_START;
    StringBuffer buffer = {0};

    while (1) {
        switch (state) {
            case STATE_START:
                if (c == EOF) {
                    token->type = TOK_EOF;
                    return 0;
                }

                if (c == '0') {
                    state = STATE_INT_ZERO;
                    break;
                }

                if (c >= '1' && c <= '9') {
                    int result = buffer_init(&buffer);
                    if (result != 0) {
                        return result;
                    }
                    result = buffer_append(&buffer, (char)c);
                    if (result != 0) {
                        buffer_free(&buffer);
                        return result;
                    }

                    state = STATE_INT_DIGITS;
                    break;
                }

                return -1;

            case STATE_INT_ZERO: {
                c = getchar();
                int result = buffer_init(&buffer);
                if (result != 0) {
                    return result;
                }
                result = buffer_append(&buffer, '0');
                if (result != 0) {
                    buffer_free(&buffer);
                    return result;
                }

                if (c >= '0' && c <= '9') {
                    result = buffer_append(&buffer, (char)c);
                    if (result != 0) {
                        buffer_free(&buffer);
                        return result;
                    }
                    state = STATE_INT_DIGITS_AFTER_ZERO;
                    break;
                }
                if (c == '_') {
                    state = STATE_INT_UNDERSCORE_AFTER_ZERO;
                    break;
                }
                if (c != EOF) {
                    ungetc(c, stdin);
                }

                token->type = TOK_INT_LITERAL;
                token->data.num_int_val = 0;
                buffer_free(&buffer);
                return 0;
            }

            case STATE_INT_DIGITS_AFTER_ZERO:
                c = getchar();
                while (c >= '0' && c <= '9') {
                    int result = buffer_append(&buffer, (char)c);
                    if (result != 0) {
                        buffer_free(&buffer);
                        return result;
                    }
                    c = getchar();
                }
                if (c == '_') {
                    state = STATE_INT_UNDERSCORE_AFTER_ZERO;
                    break;
                }

                buffer_free(&buffer);
                return -1;

            case STATE_INT_UNDERSCORE_AFTER_ZERO:
                c = getchar();
                while (c == '_') {
                    c = getchar();
                }
                if (c >= '0' && c <= '9') {
                    int result = buffer_append(&buffer, (char)c);
                    if (result != 0) {
                        buffer_free(&buffer);
                        return result;
                    }
                    state = STATE_INT_DIGITS_AFTER_ZERO;
                    break;
                }

                buffer_free(&buffer);
                return -1;

            case STATE_INT_DIGITS:
                c = getchar();
                while (c >= '0' && c <= '9') {
                    int result = buffer_append(&buffer, (char)c);
                    if (result != 0) {
                        buffer_free(&buffer);
                        return result;
                    }
                    c = getchar();
                }
                if (c == '_') {
                    state = STATE_INT_UNDERSCORE;
                    break;
                }
                if (c != EOF) {
                    ungetc(c, stdin);
                }

                token->type = TOK_INT_LITERAL;
                token->data.num_int_val = strtoll(buffer.data, NULL, 10);
                buffer_free(&buffer);
                return 0;

            case STATE_INT_UNDERSCORE:
                c = getchar();
                while (c == '_') {
                    c = getchar();
                }
                if (c >= '0' && c <= '9') {
                    int result = buffer_append(&buffer, (char)c);
                    if (result != 0) {
                        buffer_free(&buffer);
                        return result;
                    }

                    state = STATE_INT_DIGITS;
                    break;
                }

                buffer_free(&buffer);
                return -1;

            default:
                buffer_free(&buffer);
                return 42; 
        }
    }
}
