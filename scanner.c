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
    STATE_STRING_FIRST_QUOTE,
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
    STATE_NEW_LINE,
    STATE_MULTI_LINE_COMMENT,
    STATE_STRING_ESCAPE,
    STATE_STRING_BODY,
    STATE_STRING_SECOND_QUOTE,
    STATE_STRING_MULTILINE_BODY,
    STATE_STRING_MULTILINE_FIRST_QUOTE,
    STATE_STRING_MULTILINE_SECOND_QUOTE
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

static unsigned int next_line = 1;
static unsigned int next_column = 1;
static unsigned int last_line = 1;
static unsigned int last_column = 1;

static unsigned int number_of_parentheses = 0;
static bool line_has_token = false;
static bool comment_has_lf = false;
static unsigned int number_of_nested_comments = 0;
static bool reading_indentation = true;
static bool tab_in_indent = false;
static unsigned int current_indent = 0;
static bool trailing_comma_allowed = false;

static int scanner_getchar(void) {
    last_line = next_line;
    last_column = next_column;

    int c = getchar();

    if (c == '\n') {
        next_line++;
        next_column = 1;
    }
    else if (c != EOF) {
        next_column++;
    }

    return c;
}

static int scanner_ungetc(int c) {
    if (c == EOF) {
        return EOF;
    }

    int result = ungetc(c, stdin);

    if (result != EOF) {
        next_line = last_line;
        next_column = last_column;
    }

    return result;
}

static int scanner_read_token(Token *token) {
    int c = scanner_getchar();
    State state = STATE_START;
    StringBuffer buffer = {0};

    while (1) {
        switch (state) {
            case STATE_START:
                if (c == EOF) {
                    token->type = TOK_EOF;
                    return 0;
                }

                if (c == ' ') {
                    if (reading_indentation) {
                        current_indent++;
                    }
                    c = scanner_getchar();
                    break;
                }

                if (c == '\t') {
                    if (reading_indentation) {
                        tab_in_indent = true;
                    }
                    c = scanner_getchar();
                    break;
                }

                token->line = last_line;
                token->column = last_column;

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

                if (c == '"') {
                    int result = buffer_init(&buffer);
                    if (result != 0) {
                        return result;
                    }

                    state = STATE_STRING_FIRST_QUOTE;
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
                c = scanner_getchar();
                if (c == '|') {
                    token->type = TOK_OR;
                    return 0;
                }
                return -1;

            case STATE_EQUAL_SIGN:
                c = scanner_getchar();
                if (c == '=') {
                    token->type = TOK_EQUAL;
                    return 0;
                }

                if (c != EOF) {
                    scanner_ungetc(c);
                }

                token->type = TOK_ASSIGN;
                return 0;

            case STATE_LESS:
                c = scanner_getchar();
                if (c == '=') {
                    token->type = TOK_LESS_OR_EQUAL_THAN;
                    return 0;
                }

                if (c == '-') {
                    token->type = TOK_CONTROL_ARROW;
                    return 0;
                }

                if (c != EOF) {
                    scanner_ungetc(c);
                }

                token->type = TOK_LESS_THAN;
                return 0;

            case STATE_GREATER:
                c = scanner_getchar();
                if (c == '=') {
                    token->type = TOK_GREATER_OR_EQUAL_THAN;
                    return 0;
                }

                if (c != EOF) {
                    scanner_ungetc(c);
                }

                token->type = TOK_GREATER_THAN;
                return 0;

            case STATE_EXCLAMATION_MARK:
                c = scanner_getchar();
                if (c == '=') {
                    token->type = TOK_NOT_EQUAL;
                    return 0;
                }

                if (c != EOF) {
                    scanner_ungetc(c);
                }

                token->type = TOK_NOT;
                return 0;

            case STATE_AMPERSAND:
                c = scanner_getchar();
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
                c = scanner_getchar();
                if (c == '/') {
                    state = STATE_ONE_LINE_COMMENT;
                    break;
                }
                if (c == '*') {
                    state = STATE_MULTI_LINE_COMMENT;
                    number_of_nested_comments = 1;
                    comment_has_lf = false;
                    break;
                }

                if (c != EOF) {
                    scanner_ungetc(c);
                }

                token->type = TOK_DIVISION;
                return 0;

            case STATE_IDENTIFIER_UNDERSCORE:
                c = scanner_getchar();
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
                c = scanner_getchar();
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
                    scanner_ungetc(c);
                }

                token->type = keyword_or_id(buffer.data);
                if (token->type == TOK_ID) {
                    token->data.str_val = buffer.data;
                } else {
                    buffer_free(&buffer);
                }

                return 0;

            case STATE_DOT:
                c = scanner_getchar();
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
                    scanner_ungetc(c);
                }

                token->type = TOK_DOT;
                buffer_free(&buffer);
                return 0;

            case STATE_DOUBLE_DIGITS:
                c = scanner_getchar();
                while(c >= '0' && c <= '9') {
                    int result = buffer_append(&buffer, (char)c);
                    if (result != 0) {
                        buffer_free(&buffer);
                        return result;
                    }

                    c = scanner_getchar();
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
                    scanner_ungetc(c);
                }

                token->type = TOK_DOUBLE_LITERAL;
                token->data.num_float_val = strtod(buffer.data, NULL);

                buffer_free(&buffer);
                return 0;


            case STATE_DOUBLE_UNDERSCORE:
                c = scanner_getchar();
                while (c == '_') {
                    c = scanner_getchar();
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
                c = scanner_getchar();
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
                c = scanner_getchar();
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
                c = scanner_getchar();
                while(c >= '0' && c <= '9') {
                    int result = buffer_append(&buffer, (char)c);
                    if (result != 0) {
                        buffer_free(&buffer);
                        return result;
                    }

                    c = scanner_getchar();
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
                    scanner_ungetc(c);
                }

                token->type = TOK_DOUBLE_LITERAL;
                token->data.num_float_val = strtod(buffer.data, NULL);

                buffer_free(&buffer);
                return 0;

            case STATE_DOUBLE_EXPONENT_UNDERSCORE:
                c = scanner_getchar();
                while (c == '_') {
                    c = scanner_getchar();
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
                c = scanner_getchar();
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
                    scanner_ungetc(c);
                }

                token->type = TOK_INT_LITERAL;
                token->data.num_int_val = 0;
                buffer_free(&buffer);
                return 0;

            }

            case STATE_INT_DIGITS_AFTER_ZERO:
                c = scanner_getchar();
                while(c >= '0' && c <= '9') {
                    int result = buffer_append(&buffer, (char)c);
                    if (result != 0) {
                        buffer_free(&buffer);
                        return result;
                    }

                    c = scanner_getchar();
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
                c = scanner_getchar();
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
                c = scanner_getchar();
                while (c == '_') {
                    c = scanner_getchar();
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
                c = scanner_getchar();
                while (c >= '0' && c <= '9') {
                    int result = buffer_append(&buffer, (char)c);
                    if (result != 0) {
                        buffer_free(&buffer);
                        return result;
                    }
                    c = scanner_getchar();
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
                    scanner_ungetc(c);
                }
                token->type = TOK_INT_LITERAL;
                token->data.num_int_val = strtoll(buffer.data, NULL, 10);
                buffer_free(&buffer);
                return 0;

            case STATE_INT_UNDERSCORE:
                c = scanner_getchar();
                while (c == '_') {
                    c = scanner_getchar();
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

            case STATE_STRING_FIRST_QUOTE:
                c = scanner_getchar();
                if (c == '"') {
                    state = STATE_STRING_SECOND_QUOTE;
                    break;
                }
                while (c != '"') {
                    if (c == EOF || c == '\n') {
                        buffer_free(&buffer);
                        return -1;
                    }
                    if (c == '\\') {
                        state = STATE_STRING_ESCAPE;
                        break;
                    }

                    int result = buffer_append(&buffer, (char)c);
                    if (result != 0) {
                        buffer_free(&buffer);
                        return result;
                    }

                    c = scanner_getchar();
                }
                if (state == STATE_STRING_ESCAPE) {
                    break;
                }

                token->type = TOK_STRING_LITERAL;
                token->data.str_val = buffer.data;
                return 0;

            case STATE_STRING_ESCAPE:
                c = scanner_getchar();
                if (c == '"' || c == '\\') {
                    int result = buffer_append(&buffer, (char)c);
                        if (result != 0) {
                            buffer_free(&buffer);
                            return result;
                        }

                    state = STATE_STRING_BODY;
                    break;
                }
                else if (c == 'n') {
                    c = '\n';
                    int result = buffer_append(&buffer, (char)c);
                        if (result != 0) {
                            buffer_free(&buffer);
                            return result;
                        }
                    state = STATE_STRING_BODY;
                    break;
                }
                else if (c == 't') {
                    c = '\t';
                    int result = buffer_append(&buffer, (char)c);
                        if (result != 0) {
                            buffer_free(&buffer);
                            return result;
                        }
                    state = STATE_STRING_BODY;
                    break;
                }
                else if (c == 'r') {
                    c = '\r';
                    int result = buffer_append(&buffer, (char)c);
                        if (result != 0) {
                            buffer_free(&buffer);
                            return result;
                        }
                    state = STATE_STRING_BODY;
                    break;
                }
                buffer_free(&buffer);
                return -1;

            case STATE_STRING_BODY:
                c = scanner_getchar();
                while (c != '"') {
                    if (c == EOF || c == '\n') {
                        buffer_free(&buffer);
                        return -1;
                    }
                    if (c == '\\') {
                        state = STATE_STRING_ESCAPE;
                        break;
                    }

                    int result = buffer_append(&buffer, (char)c);
                    if (result != 0) {
                        buffer_free(&buffer);
                        return result;
                    }

                    c = scanner_getchar();
                }
                if (state == STATE_STRING_ESCAPE) {
                    break;
                }

                token->type = TOK_STRING_LITERAL;
                token->data.str_val = buffer.data;
                return 0;


            case STATE_STRING_SECOND_QUOTE:
                c = scanner_getchar();
                if (c == '"') {
                    state = STATE_STRING_MULTILINE_BODY;
                    break;
                }

                if (c != EOF) {
                    scanner_ungetc(c);
                }
                token->type = TOK_STRING_LITERAL;
                token->data.str_val = buffer.data;
                return 0;

            case STATE_STRING_MULTILINE_BODY:
                c = scanner_getchar();
                while (c != '"') {
                    if (c == EOF) {
                        buffer_free(&buffer);
                        return -1;
                    }

                    int result = buffer_append(&buffer, (char)c);
                    if (result != 0) {
                        buffer_free(&buffer);
                        return result;
                    }

                    c = scanner_getchar();
                }
                state = STATE_STRING_MULTILINE_FIRST_QUOTE;
                break;


            case STATE_STRING_MULTILINE_FIRST_QUOTE: {
                c = scanner_getchar();
                if (c == EOF) {
                    buffer_free(&buffer);
                    return -1;
                }
                if (c == '"') {
                    state = STATE_STRING_MULTILINE_SECOND_QUOTE;
                    break;
                }
                if (c != EOF) {
                    scanner_ungetc(c);
                }

                int result = buffer_append(&buffer, '"');
                if (result != 0) {
                    buffer_free(&buffer);
                    return result;
                }

                state = STATE_STRING_MULTILINE_BODY;
                break;
            }

            case STATE_STRING_MULTILINE_SECOND_QUOTE: {
                c = scanner_getchar();
                if (c == EOF) {
                    buffer_free(&buffer);
                    return -1;
                    }
                if (c == '"') {
                    token->type = TOK_STRING_LITERAL;
                    token->data.str_val = buffer.data;
                    return 0;
                }

                int result = buffer_append(&buffer, '"');
                if (result != 0) {
                    buffer_free(&buffer);
                    return result;
                }

                result = buffer_append(&buffer, '"');
                if (result != 0) {
                    buffer_free(&buffer);
                    return result;
                }

                if (c != EOF) {
                    scanner_ungetc(c);
                }

                state = STATE_STRING_MULTILINE_BODY;
                break;
            }

            case STATE_ONE_LINE_COMMENT:
                c = scanner_getchar();
                while (c != '\n' && c != EOF) {
                    c = scanner_getchar();
                }

                if (c == EOF) {
                    token->type = TOK_EOF;
                    return 0;
                }

                state = STATE_NEW_LINE;
                break;

            case STATE_NEW_LINE:
                reading_indentation = true;
                current_indent = 0;
                tab_in_indent = false;
                trailing_comma_allowed = true;

                if (number_of_parentheses == 0 && line_has_token) {
                    token->type = TOK_EOL;
                    line_has_token = false;
                    return 0;
                }

                c = scanner_getchar();
                if (c == EOF) {
                    token->type = TOK_EOF;
                    return 0;
                }

                state = STATE_START;
                break;

            case STATE_MULTI_LINE_COMMENT:
                c = scanner_getchar();
                while (1) {
                    while (c != '*' && c != EOF && c != '/' && c != '\n') {
                        c = scanner_getchar();
                    }
                    if (c == EOF) {
                        return -1;
                    }
                    if (c == '\n') {
                        comment_has_lf = true;
                        c = scanner_getchar();
                        continue;
                    }
                    if (c == '/') {
                        c = scanner_getchar();
                        if (c == EOF) {
                            return -1;
                        }
                        if (c == '*') {
                            number_of_nested_comments++;
                            c = scanner_getchar();
                            continue;
                        }
                    }
                    if (c == '*') {
                        c = scanner_getchar();
                        if (c == EOF) {
                            return -1;
                        }
                        if (c == '/') {
                            number_of_nested_comments--;
                            if (number_of_nested_comments == 0) {
                                break;
                            }

                            c = scanner_getchar();
                            continue;
                        }
                    }
                }
                c = scanner_getchar();

                if (comment_has_lf == true) {
                    while (c == ' ' || c == '\t') {
                        c = scanner_getchar();
                    }
                    if (c == '\n') {
                        state = STATE_NEW_LINE;
                        break;
                    }
                    if (c == EOF) {
                        token->type = TOK_EOF;
                        return 0;
                    }
                    return -1;
                }
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
    token->indent = 0;
    token->lf_before = false;

    int result = scanner_read_token(token);

    if (result == 0 && (token->type == TOK_EOL || token->type == TOK_EOF)) {
        token->line = last_line;
        token->column = last_column;
    }

    if (result == 0 && token->type != TOK_EOF && token->type != TOK_EOL) {
        if (tab_in_indent) {
            if (token->type == TOK_ID || token->type == TOK_STRING_LITERAL) {
                free(token->data.str_val);
                token->data.str_val = NULL;
            }
            return -1;
        }

        token->lf_before = trailing_comma_allowed;
        trailing_comma_allowed = false;

        line_has_token = true;
        reading_indentation = false;
        token->indent = current_indent;
    }

    return result;
}
