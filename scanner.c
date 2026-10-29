#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
    STATE_INT_UNDERSCORE_AFTER_ZERO,
    STATE_DOUBLE_DIGITS,
    STATE_DOUBLE_UNDERSCORE,
    STATE_DOUBLE_SUFFIX,
    STATE_DOUBLE_EXPONENT,
    STATE_DOUBLE_EXPONENT_SIGN,
    STATE_DOUBLE_EXPONENT_DIGITS,
    STATE_DOUBLE_EXPONENT_UNDERSCORE,
    STATE_DOUBLE_AFTER_DOT,
    STATE_IDENTIFIER_UNDERSCORE,
    STATE_IDENTIFIER,
    STATE_EQUAL_SIGN,
    STATE_EXCLAMATION_MARK,
    STATE_ONE_LINE_COMMENT,
    STATE_NEW_LINE
} State;

static TokenType keyword_or_id(const char *text) {
    if (strcmp(text, "var") == 0)       return TOK_KW_VAR;
    if (strcmp(text, "def") == 0)       return TOK_KW_DEF;
    if (strcmp(text, "do") == 0)        return TOK_KW_DO;
    if (strcmp(text, "Double") == 0)    return TOK_KW_DOUBLE;
    if (strcmp(text, "else") == 0)      return TOK_KW_ELSE;
    if (strcmp(text, "if") == 0)        return TOK_KW_IF;
    if (strcmp(text, "import") == 0)    return TOK_KW_IMPORT;
    if (strcmp(text, "Int") == 0)       return TOK_KW_INT;
    if (strcmp(text, "return") == 0)    return TOK_KW_RETURN;
    if (strcmp(text, "String") == 0)    return TOK_KW_STRING;
    if (strcmp(text, "then") == 0)      return TOK_KW_THEN;
    if (strcmp(text, "Unit") == 0)      return TOK_KW_UNIT;
    if (strcmp(text, "val") == 0)       return TOK_KW_VAL;
    if (strcmp(text, "while") == 0)     return TOK_KW_WHILE;
    if (strcmp(text, "Boolean") == 0)   return TOK_KW_BOOLEAN;
    if (strcmp(text, "for") == 0)       return TOK_KW_FOR;
    if (strcmp(text, "extension") == 0) return TOK_KW_EXTENSION;
    if (strcmp(text, "true") == 0)      return TOK_TRUE;
    if (strcmp(text, "false") == 0)     return TOK_FALSE;

    return TOK_ID;
}

static unsigned int number_of_parentheses = 0;
static bool line_has_token = false;

static int scanner_read_token(Token *token) {
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

                if (c == '\n') {
                    state = STATE_NEW_LINE;
                    break;
                }

                if (c == '+') {
                    state = STATE_PLUS;
                    break;
                }

                if (c == '-') {
                    state = STATE_MINUS;
                    break;
                }

                if (c == '/') {
                    state = STATE_SLASH;
                    break;
                }

                if (c == '*') {
                    state = STATE_MULTIPLY;
                    break;
                }

                if (c == '<') {
                    state = STATE_LESS;
                    break;
                }

                if (c == '>') {
                    state = STATE_GREATER;
                    break;
                }

                if (c == '(') {
                    state = STATE_LEFT_PARENTHESES;
                    break;
                }

                if (c == ')') {
                    state = STATE_RIGHT_PARENTHESES;
                    break;
                }

                if (c == ',') {
                    state = STATE_COMMA;
                    break;
                }

                if (c == ':') {
                    state = STATE_COLON;
                    break;
                }

                if (c == ';') {
                    state = STATE_SEMICOLON;
                    break;
                }

                if (c == '@') {
                    state = STATE_AT;
                    break;
                }

                if (c == '&') {
                    state = STATE_AMPERSAND;
                    break;
                }

                if (c == '|') {
                    state = STATE_VERTICAL_BAR;
                    break;
                }

                if (c == '=') {
                    state = STATE_EQUAL_SIGN;
                    break;
                }

                if (c == '!') {
                    state = STATE_EXCLAMATION_MARK;
                    break;
                }

                if (c == '.') {
                    int result = buffer_init(&buffer);
                    if (result != 0) {
                        return result;
                    }
                    result = buffer_append(&buffer, (char)c);
                    if (result != 0) {
                        buffer_free(&buffer);
                        return result;
                    }
                    state = STATE_DOT;
                    break;
                }

                if (c == '_') {
                    int result = buffer_init(&buffer);
                    if (result != 0) {
                        return result;
                    }
                    result = buffer_append(&buffer, (char)c);
                    if (result != 0) {
                        buffer_free(&buffer);
                        return result;
                    }

                    state = STATE_IDENTIFIER_UNDERSCORE;
                    break;
                }

                if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
                    int result = buffer_init(&buffer);
                    if (result != 0) {
                        return result;
                    }
                    result = buffer_append(&buffer, (char)c);
                    if (result != 0) {
                        buffer_free(&buffer);
                        return result;
                    }

                    state = STATE_IDENTIFIER;
                    break;
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

            case STATE_PLUS:
                token->type = TOK_PLUS;
                return 0;

            case STATE_MINUS:
                token->type = TOK_MINUS;
                return 0;

            case STATE_MULTIPLY:
                token->type = TOK_MULTIPLY;
                return 0;

            case STATE_VERTICAL_BAR:
                c = getchar();
                if (c == '|') {
                    token->type = TOK_OR;
                    return 0;
                }
                return -1;

            case STATE_EQUAL_SIGN:
                c = getchar();
                if (c == '=') {
                    token->type = TOK_EQUAL;
                    return 0;
                }

                if (c != EOF) {
                    ungetc(c, stdin);
                }

                token->type = TOK_ASSIGN;
                return 0;

            case STATE_LESS:
                c = getchar();
                if (c == '=') {
                    token->type = TOK_LESS_OR_EQUAL_THAN;
                    return 0;
                }

                if (c == '-') {
                    token->type = TOK_CONTROL_ARROW;
                    return 0;
                }

                if (c != EOF) {
                    ungetc(c, stdin);
                }

                token->type = TOK_LESS_THAN;
                return 0;

            case STATE_GREATER:
                c = getchar();
                if (c == '=') {
                    token->type = TOK_GREATER_OR_EQUAL_THAN;
                    return 0;
                }

                if (c != EOF) {
                    ungetc(c, stdin);
                }

                token->type = TOK_GREATER_THAN;
                return 0;

            case STATE_EXCLAMATION_MARK:
                c = getchar();
                if (c == '=') {
                    token->type = TOK_NOT_EQUAL;
                    return 0;
                }

                if (c != EOF) {
                    ungetc(c, stdin);
                }

                token->type = TOK_NOT;
                return 0;

            case STATE_AMPERSAND:
                c = getchar();
                if (c == '&') {
                    token->type = TOK_AND;
                    return 0;
                }

                return -1;

            case STATE_LEFT_PARENTHESES:
                token->type = TOK_LEFT_PARENTHESIS;
                number_of_parentheses++;
                return 0;

            case STATE_RIGHT_PARENTHESES:
                token->type = TOK_RIGHT_PARENTHESIS;
                if (number_of_parentheses > 0) {
                    number_of_parentheses--;
                }
                return 0;

            case STATE_COMMA:
                token->type = TOK_COMMA;
                return 0;

            case STATE_COLON:
                token->type = TOK_COLON;
                return 0;

            case STATE_SEMICOLON:
                token->type = TOK_SEMICOLON;
                return 0;

            case STATE_AT:
                token->type = TOK_AT;
                return 0;

            case STATE_SLASH:
                c = getchar();
                if (c == '/') {
                    state = STATE_ONE_LINE_COMMENT;
                    break;
                }

                if (c != EOF) {
                    ungetc(c, stdin);
                }

                token->type = TOK_DIVISION;
                return 0;

            case STATE_IDENTIFIER_UNDERSCORE:
                c = getchar();
                if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c == '_') || (c >= '0' && c <= '9')) {
                    int result = buffer_append(&buffer, (char)c);
                    if (result != 0) {
                        buffer_free(&buffer);
                        return result;
                    }

                    state = STATE_IDENTIFIER;
                    break;
                }

                buffer_free(&buffer);
                return -1;

            case STATE_IDENTIFIER:
                c = getchar();
                if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c == '_') || (c >= '0' && c <= '9')) {
                    int result = buffer_append(&buffer, (char)c);
                    if (result != 0) {
                        buffer_free(&buffer);
                        return result;
                    }

                    state = STATE_IDENTIFIER;
                    break;
                }

                if (c != EOF) {
                    ungetc(c, stdin);
                }

                token->type = keyword_or_id(buffer.data);
                if (token->type == TOK_ID) {
                    token->data.str_val = buffer.data;
                } else {
                    buffer_free(&buffer);
                }

                return 0;

            case STATE_DOT:
                c = getchar();
                if (c >= '0' && c <= '9') {
                    int result = buffer_append(&buffer, (char)c);
                    if (result != 0) {
                        buffer_free(&buffer);
                        return result;
                    }
                    state = STATE_DOUBLE_DIGITS;
                    break;
                }
                if (c != EOF) {
                    ungetc(c, stdin);
                }

                token->type = TOK_DOT;
                buffer_free(&buffer);
                return 0;

            case STATE_DOUBLE_DIGITS:
                c = getchar();
                while(c >= '0' && c <= '9') {
                    int result = buffer_append(&buffer, (char)c);
                    if (result != 0) {
                        buffer_free(&buffer);
                        return result;
                    }

                    c = getchar();
                }
                if (c == '_') {
                    state = STATE_DOUBLE_UNDERSCORE;
                    break;
                }
                else if (c == 'D' || c == 'd') {
                    state = STATE_DOUBLE_SUFFIX;
                    break;
                }
                else if (c == 'E' || c == 'e') {
                    int result = buffer_append(&buffer, (char)c);
                    if (result != 0) {
                        buffer_free(&buffer);
                        return result;
                    }

                    state = STATE_DOUBLE_EXPONENT;
                    break;
                }
                if (c != EOF) {
                    ungetc(c, stdin);
                }

                token->type = TOK_DOUBLE_LITERAL;
                token->data.num_float_val = strtod(buffer.data, NULL);

                buffer_free(&buffer);
                return 0;


            case STATE_DOUBLE_UNDERSCORE:
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

                        state = STATE_DOUBLE_DIGITS;
                        break;
                    }

                buffer_free(&buffer);
                return -1;

            case STATE_DOUBLE_SUFFIX:
                token->type = TOK_DOUBLE_LITERAL;
                token->data.num_float_val = strtod(buffer.data, NULL);
                buffer_free(&buffer);
                return 0;

            case STATE_DOUBLE_EXPONENT:
                c = getchar();
                if (c == '+' || c == '-') {
                    int result = buffer_append(&buffer, (char)c);
                    if (result != 0) {
                        buffer_free(&buffer);
                        return result;
                    }

                    state = STATE_DOUBLE_EXPONENT_SIGN;
                    break;
                }
                else if (c >= '0' && c <= '9') {
                    int result = buffer_append(&buffer, (char)c);
                    if (result != 0) {
                        buffer_free(&buffer);
                        return result;
                    }
                        state = STATE_DOUBLE_EXPONENT_DIGITS;
                        break;
                }

                buffer_free(&buffer);
                return -1;

            case STATE_DOUBLE_EXPONENT_SIGN:
                c = getchar();
                if (c >= '0' && c <= '9') {
                    int result = buffer_append(&buffer, (char)c);
                    if (result != 0) {
                        buffer_free(&buffer);
                        return result;
                    }

                    state = STATE_DOUBLE_EXPONENT_DIGITS;
                    break;
                }
                buffer_free(&buffer);
                return -1;

            case STATE_DOUBLE_EXPONENT_DIGITS:
                c = getchar();
                while(c >= '0' && c <= '9') {
                    int result = buffer_append(&buffer, (char)c);
                    if (result != 0) {
                        buffer_free(&buffer);
                        return result;
                    }

                    c = getchar();
                }
                if (c == '_') {
                    state = STATE_DOUBLE_EXPONENT_UNDERSCORE;
                    break;
                }
                else if (c == 'D' || c == 'd') {
                    state = STATE_DOUBLE_SUFFIX;
                    break;
                }
                if (c != EOF) {
                    ungetc(c, stdin);
                }

                token->type = TOK_DOUBLE_LITERAL;
                token->data.num_float_val = strtod(buffer.data, NULL);

                buffer_free(&buffer);
                return 0;

            case STATE_DOUBLE_EXPONENT_UNDERSCORE:
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

                    state = STATE_DOUBLE_EXPONENT_DIGITS;
                    break;
                }
                buffer_free(&buffer);
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
                    int result = buffer_append(&buffer, (char)c);
                    if (result != 0) {
                        buffer_free(&buffer);
                        return result;
                    }
                    state = STATE_INT_DIGITS_AFTER_ZERO;
                    break;
                }
                if (c == '.') {
                    int result = buffer_append(&buffer, (char)c);
                    if (result != 0) {
                        buffer_free(&buffer);
                        return result;
                    }
                    state = STATE_DOUBLE_AFTER_DOT;
                    break;
                }
                if (c == 'e' || c == 'E') {
                    int result = buffer_append(&buffer, (char)c);
                    if (result != 0) {
                        buffer_free(&buffer);
                        return result;
                    }
                    state = STATE_DOUBLE_EXPONENT;
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
                while(c >= '0' && c <= '9') {
                    int result = buffer_append(&buffer, (char)c);
                    if (result != 0) {
                        buffer_free(&buffer);
                        return result;
                    }

                    c = getchar();
                }
                if (c == 'e' || c == 'E') {
                    int result = buffer_append(&buffer, (char)c);
                    if (result != 0) {
                        buffer_free(&buffer);
                        return result;
                    }
                    state = STATE_DOUBLE_EXPONENT;
                    break;
                }
                if (c == '.') {
                    int result = buffer_append(&buffer, (char)c);
                    if (result != 0) {
                        buffer_free(&buffer);
                        return result;
                    }
                    state = STATE_DOUBLE_AFTER_DOT;
                    break;
                }
                if (c == '_') {
                    state = STATE_INT_UNDERSCORE_AFTER_ZERO;
                    break;
                }

                buffer_free(&buffer);
                return -1;

            case STATE_DOUBLE_AFTER_DOT:
                c = getchar();
                if (c >= '0' && c <= '9') {
                    int result = buffer_append(&buffer, (char)c);
                    if (result != 0) {
                        buffer_free(&buffer);
                        return result;
                    }
                    state = STATE_DOUBLE_DIGITS;
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
                if (c == '.') {
                    int result = buffer_append(&buffer, (char)c);
                    if (result != 0) {
                        buffer_free(&buffer);
                        return result;
                    }
                    state = STATE_DOUBLE_AFTER_DOT;
                    break;
                }
                if (c == 'e' || c == 'E') {
                    int result = buffer_append(&buffer, (char)c);
                    if (result != 0) {
                        buffer_free(&buffer);
                        return result;
                    }
                    state = STATE_DOUBLE_EXPONENT;
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

            case STATE_ONE_LINE_COMMENT:
                c = getchar();
                while (c != '\n' && c != EOF) {
                    c = getchar();
                }

                if (c == EOF) {
                    token->type = TOK_EOF;
                    return 0;
                }

                state = STATE_NEW_LINE;
                break;

            case STATE_NEW_LINE:
                if (number_of_parentheses == 0 && line_has_token) {
                    token->type = TOK_EOL;
                    line_has_token = false;
                    return 0;
                }

                c = getchar();
                if (c == EOF) {
                    token->type = TOK_EOF;
                    return 0;
                }

                state = STATE_START;
                break;

            default:
                buffer_free(&buffer);
                return 42; 
        }
    }
}

int scanner_get_token(Token *token) {
    int result = scanner_read_token(token);

    if (result == 0 && token->type != TOK_EOF && token->type != TOK_EOL) {
        line_has_token = true;
    }

    return result;
}
