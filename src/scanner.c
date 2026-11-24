/**
 * @file scanner.c
 * @brief Lexical analyzer for IFJ26 to IFJcode26
 * @author David Zoblivý (xzoblid00)
 * @date 14.11.2026
 */

#include "scanner.h"
#include "scanner_buffer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef enum {
    // Start
    STATE_START,                    

    // Identifiers
    STATE_IDENTIFIER,
    STATE_IDENTIFIER_UNDERSCORE,

    // Integer literals
    STATE_INT_ZERO,
    STATE_INT_DIGITS,
    STATE_INT_UNDERSCORE,
    STATE_INT_DIGITS_AFTER_ZERO,
    STATE_INT_UNDERSCORE_AFTER_ZERO,

    // Decimal literals
    STATE_DOT,
    STATE_NUMBER_AFTER_DOT,
    STATE_DOUBLE_DIGITS,
    STATE_DOUBLE_UNDERSCORE,
    STATE_DOUBLE_EXPONENT,
    STATE_DOUBLE_EXPONENT_SIGN,
    STATE_DOUBLE_EXPONENT_DIGITS,
    STATE_DOUBLE_EXPONENT_UNDERSCORE,
    STATE_DOUBLE_SUFFIX,

    // String literals
    STATE_STRING_FIRST_QUOTE,
    STATE_STRING_SECOND_QUOTE,
    STATE_STRING_BODY,
    STATE_STRING_ESCAPE,
    STATE_STRING_MULTILINE_BODY,
    STATE_STRING_MULTILINE_FIRST_QUOTE,
    STATE_STRING_MULTILINE_SECOND_QUOTE,
    STATE_STRING_MULTILINE_CLOSING_QUOTES,

    // Operators and separators
    STATE_PLUS,
    STATE_MINUS,
    STATE_MULTIPLY,
    STATE_SLASH,
    STATE_LESS,
    STATE_GREATER,
    STATE_EQUAL_SIGN,
    STATE_EXCLAMATION_MARK,
    STATE_AMPERSAND,
    STATE_VERTICAL_BAR,
    STATE_LEFT_PARENTHESES,
    STATE_RIGHT_PARENTHESES,
    STATE_COMMA,
    STATE_COLON,
    STATE_SEMICOLON,
    STATE_AT,

    // Comments
    STATE_ONE_LINE_COMMENT,
    STATE_MULTI_LINE_COMMENT,

    // End of line
    STATE_NEW_LINE
} State;

typedef enum {
    INTERPOLATED_MODE_NONE,
    INTERPOLATED_MODE_SINGLE_LINE,
    INTERPOLATED_MODE_MULTILINE,
    INTERPOLATED_MODE_ENDED
} InterpolatedMode;

typedef struct interpolation_state {
    InterpolatedMode mode;
    bool reading_expression;
    struct interpolation_state *previous;
} InterpolationState;

typedef enum {
    STDERR_NONE,
    STDERR_UNKNOWN_CHARACTER,
    STDERR_INVALID_IDENTIFIER,
    STDERR_INVALID_NUMBER,
    STDERR_INVALID_ESCAPE,
    STDERR_INVALID_INTERPOLATION,
    STDERR_OPEN_STRING,
    STDERR_OPEN_COMMENT,
    STDERR_OPEN_INTERPOLATION,
    STDERR_TAB_IN_INDENT,
    STDERR_TOKEN_AFTER_COMMENT,
    STDERR_MEMORY_ERROR,
    STDERR_STDIN_ERROR,
    STDERR_INVALID_STATE
} StderrMessage;

/**
 * @brief checks whether the input is a keyword or an identifier
 * @param[in] text word to classify
 * @return TOK_KW_*** if keyword is found, otherwise TOK_ID
 */
static TokenType keyword_or_id(const char *text) {
    if (strcmp(text, "var") == 0) {
        return TOK_KW_VAR;
    }
    if (strcmp(text, "def") == 0) {
        return TOK_KW_DEF;
    }
    if (strcmp(text, "do") == 0) {
        return TOK_KW_DO;
    }
    if (strcmp(text, "Double") == 0) {
        return TOK_KW_DOUBLE;
    }
    if (strcmp(text, "else") == 0) {
        return TOK_KW_ELSE;
    }
    if (strcmp(text, "if") == 0) {
        return TOK_KW_IF;
    }
    if (strcmp(text, "import") == 0) {
        return TOK_KW_IMPORT;
    }
    if (strcmp(text, "Int") == 0) {
        return TOK_KW_INT;
    }
    if (strcmp(text, "return") == 0) {
        return TOK_KW_RETURN;
    }
    if (strcmp(text, "String") == 0) {
        return TOK_KW_STRING;
    }
    if (strcmp(text, "then") == 0) {
        return TOK_KW_THEN;
    }
    if (strcmp(text, "Unit") == 0) {
        return TOK_KW_UNIT;
    }
    if (strcmp(text, "val") == 0) {
        return TOK_KW_VAL;
    }
    if (strcmp(text, "while") == 0) {
        return TOK_KW_WHILE;
    }
    if (strcmp(text, "Boolean") == 0) {
        return TOK_KW_BOOLEAN;
    }
    if (strcmp(text, "for") == 0) {
        return TOK_KW_FOR;
    }
    if (strcmp(text, "extension") == 0) {
        return TOK_KW_EXTENSION;
    }
    if (strcmp(text, "true") == 0) {
        return TOK_TRUE;
    }
    if (strcmp(text, "false") == 0) {
        return TOK_FALSE;
    }

    return TOK_ID;
}

// position tracking
static unsigned int next_line = 1;
static unsigned int next_column = 1;
static unsigned int last_line = 1;
static unsigned int last_column = 1;

// nesting flags
static unsigned int parentheses_depth = 0;
static unsigned int comments_depth = 0;
static InterpolationState current_interpolation = {0};

// indentation
static bool reading_indentation = true;     // count spaces before the first token
static unsigned int current_indent = 0;     // counted space indentation
static bool tab_in_indent = false;          // if there is a tab in indentation before token -> lexical error

// line ending
static bool line_has_token = false;         // ignore lines that dont have token
static bool comment_has_lf = false;         // checks for tokens after end of multiline comment
static bool trailing_comma_allowed = false; // in parentheses after comma there can be a NEWLINE

// pending token
static bool processing_dot = false;          // VALMEM extension

/**
 * @brief prints an error message to stderr
 * @param[in] message error to report
 * @return SCANNER_OK for STDERR_NONE, otherwise corresponding error code
 */
static ScannerErrorCode scanner_error_message(StderrMessage message) {
    switch (message) {
        case STDERR_NONE:
            return SCANNER_OK;

        case STDERR_UNKNOWN_CHARACTER:
            fprintf(stderr, "l:%u c:%u - unknown character!\n", last_line, last_column);
            return SCANNER_LEXICAL_ERROR;

        case STDERR_INVALID_IDENTIFIER:
            fprintf(stderr, "l:%u c:%u - invalid identifier!\n", last_line, last_column);
            return SCANNER_LEXICAL_ERROR;

        case STDERR_INVALID_NUMBER:
            fprintf(stderr, "l:%u c:%u - invalid number!\n", last_line, last_column);
            return SCANNER_LEXICAL_ERROR;
            
        case STDERR_INVALID_ESCAPE:
            fprintf(stderr, "l:%u c:%u - invalid escape sequence!\n", last_line, last_column);
            return SCANNER_LEXICAL_ERROR;
        
        case STDERR_INVALID_INTERPOLATION:
            fprintf(stderr, "l:%u c:%u - invalid interpolation!\n", last_line, last_column);
            return SCANNER_LEXICAL_ERROR;
            
        case STDERR_OPEN_STRING:
            fprintf(stderr, "l:%u c:%u - string is not ended!\n", last_line, last_column);
            return SCANNER_LEXICAL_ERROR;

        case STDERR_OPEN_COMMENT:
            fprintf(stderr, "l:%u c:%u - comment is not ended!\n", last_line, last_column);
            return SCANNER_LEXICAL_ERROR;

        case STDERR_OPEN_INTERPOLATION:
            fprintf(stderr, "l:%u c:%u - interpolation is not ended!\n", last_line, last_column);
            return SCANNER_LEXICAL_ERROR;

        case STDERR_TAB_IN_INDENT:
            fprintf(stderr, "l:%u c:%u - there is tab in the indentation!\n", last_line, last_column);
            return SCANNER_LEXICAL_ERROR;

        case STDERR_TOKEN_AFTER_COMMENT:
            fprintf(stderr, "l:%u c:%u - there is a token after a comment!\n", last_line, last_column);
            return SCANNER_LEXICAL_ERROR;

        case STDERR_MEMORY_ERROR:
            fprintf(stderr, "memory allocation has failed!\n");
            return SCANNER_MEMORY_ERROR;

        case STDERR_STDIN_ERROR:
            fprintf(stderr, "reading has failed!\n");
            return SCANNER_READ_ERROR;

        default:
            fprintf(stderr, "internal scanner error has failed!\n");
            return SCANNER_INVALID_STATE_ERROR;
    }
}

/**
 * @brief reads the next character from stdin and updates the position tracker
 * @return character value as int, or EOF at end of input or on a read error
 */
static int scanner_getchar(void) {
    last_line = next_line;
    last_column = next_column;

    int c = getchar();

    if (c == '\n') {
        next_line++;
        next_column = 1;
    } else if (c != EOF) {
        next_column++;
    }

    return c;
}

/**
 * @brief return the most recently read character to stdin and restore the tracker position
 * @param[in] c character to return to stdin
 * @return character that was returned, or EOF if return failed or the character is EOF
 */
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

/**
 * @brief pushes the current interpolation state to a stack
 * @return STDERR_NONE on success, otherwise STDERR_MEMORY_ERROR
 */
static StderrMessage interpolation_push(void) {
    InterpolationState *stack_node = malloc(sizeof(*stack_node));
    if (stack_node == NULL) {
        return STDERR_MEMORY_ERROR;
    }

    *stack_node = current_interpolation;
    current_interpolation.previous = stack_node;

    return STDERR_NONE;
}

/** 
 * @brief pop and restore the last saved interpolation state out of a stack 
 * @return STDERR_NONE on success, otherwise STDERR_INVALID_STATE if stack is empty
*/
static StderrMessage interpolation_pop(void) {
    if (current_interpolation.previous == NULL) {
        return STDERR_INVALID_STATE;
    }

    InterpolationState *stack_node = current_interpolation.previous;
    current_interpolation = *stack_node;

    free(stack_node);
    return STDERR_NONE;
}

/**
 * @brief free the interpolation state stack and reset the current state
 */
void interpolation_free(void) {
    while (current_interpolation.previous != NULL) {
        InterpolationState *stack_node = current_interpolation.previous;
        current_interpolation.previous = stack_node->previous;

        free(stack_node);
    }

    current_interpolation.mode = INTERPOLATED_MODE_NONE;
    current_interpolation.reading_expression = false;
}

/**
 * @brief processes part of the interpolated string and creates one token
 * @param[out] token receives the token data on success
 * @return STDERR_NONE on success, otherwise the specific scanner error 
 */
static StderrMessage scanner_read_interpolated_text(Token *token) {
    if (current_interpolation.mode == INTERPOLATED_MODE_ENDED) {
        token->type = TOK_INTERPOLATED_END;
        token->line = next_line;
        token->column = next_column - 3;
        return interpolation_pop();
    }

    StringBuffer buffer = {0};
    ScannerErrorCode result = buffer_init(&buffer);
    if (result != SCANNER_OK) {
        return STDERR_MEMORY_ERROR;
    }

    int c = scanner_getchar();
    token->line = last_line;
    token->column = last_column;

    while (1) {
        if (c == '$' || c == '"') {
            break;
        }

        if (c == '\\' && current_interpolation.mode == INTERPOLATED_MODE_SINGLE_LINE) {
            break;
        }

        if (c == EOF || (c == '\n' && current_interpolation.mode == INTERPOLATED_MODE_SINGLE_LINE)) {
            buffer_free(&buffer);
            return STDERR_OPEN_STRING;
        }

        result = buffer_append(&buffer, (char)c);
        if (result != SCANNER_OK) {
            buffer_free(&buffer);
            return STDERR_MEMORY_ERROR;
        }

        c = scanner_getchar();
    }

    if ((c == '"' || c == '$') && buffer.length > 0) {
        scanner_ungetc(c);

        token->type = TOK_STRING_LITERAL;
        token->data.str_val = buffer.data;

        return STDERR_NONE;
    }

    if (c == '"' && buffer.length == 0 && current_interpolation.mode == INTERPOLATED_MODE_SINGLE_LINE) {
        buffer_free(&buffer);

        token->type = TOK_INTERPOLATED_END;

        return interpolation_pop();
    }

    if (c == '"' && current_interpolation.mode == INTERPOLATED_MODE_MULTILINE) {
        while (c == '"') {
            result = buffer_append(&buffer, (char)c);
            if (result != SCANNER_OK) {
                buffer_free(&buffer);
                return STDERR_MEMORY_ERROR;
            }

            c = scanner_getchar();
        }

        scanner_ungetc(c);

        if (buffer.length >= 3) {
            buffer.length -= 3;
            buffer.data[buffer.length] = '\0';
            current_interpolation.mode = INTERPOLATED_MODE_ENDED;
        } else if (c == EOF) {
            buffer_free(&buffer);
            return STDERR_OPEN_STRING;
        }

        token->type = TOK_STRING_LITERAL;
        token->data.str_val = buffer.data;

        return STDERR_NONE;
    }

    if (c == '$') {
        c = scanner_getchar();

        if (c == '$') {
            result = buffer_append(&buffer, (char)c);
            if (result != SCANNER_OK) {
                buffer_free(&buffer);
                return STDERR_MEMORY_ERROR;
            }

            token->type = TOK_STRING_LITERAL;
            token->data.str_val = buffer.data;
            return STDERR_NONE;
        }

        if (c == '{') {
            buffer_free(&buffer);

            token->type = TOK_INTERPOLATION_BEGIN;
            current_interpolation.reading_expression = true;

            return STDERR_NONE;
        }

        if (isalpha(c) || (c == '_')) {
            ScannerErrorCode result = buffer_append(&buffer, (char)c);
            if (result != SCANNER_OK) {
                buffer_free(&buffer);
                return STDERR_MEMORY_ERROR;
            }

            c = scanner_getchar();
            while (isalnum(c) || (c == '_')) {
                ScannerErrorCode result = buffer_append(&buffer, (char)c);
                if (result != SCANNER_OK) {
                    buffer_free(&buffer);
                    return STDERR_MEMORY_ERROR;
                }
                c = scanner_getchar();
            }

            scanner_ungetc(c);

            if (buffer.length == 1 && buffer.data[0] == '_') {
                buffer_free(&buffer);
                return STDERR_INVALID_IDENTIFIER;
            }

            token->type = keyword_or_id(buffer.data);
            if (token->type == TOK_ID) {
                token->data.str_val = buffer.data;
            } else {
                buffer_free(&buffer);
            }
            return STDERR_NONE;
        }

        buffer_free(&buffer);
        return STDERR_INVALID_IDENTIFIER;
    } else if (c == '\\') {
        c = scanner_getchar();
        if (c == 'n') {
            c = '\n';
        } else if (c == 'r') {
            c = '\r';
        } else if (c == 't') {
            c = '\t';
        } else if (c != '"' && c != '\\') {
            buffer_free(&buffer);
            return STDERR_INVALID_ESCAPE;
        }

        result = buffer_append(&buffer, (char)c);
        if (result != SCANNER_OK) {
            buffer_free(&buffer);
            return STDERR_MEMORY_ERROR;
        }

        token->type = TOK_STRING_LITERAL;
        token->data.str_val = buffer.data;
        return STDERR_NONE;
    }

    buffer_free(&buffer);
    return STDERR_INVALID_STATE;
}

/**
 * @brief reads the next token using the finite state machine
 * @param[out] token receives the token data on success
 * @return STDERR_NONE on success, otherwise the specific scanner error 
 */
static StderrMessage scanner_read_token(Token *token) {
    if (processing_dot == true) {
        token->type = TOK_DOT;
        token->line = next_line;
        token->column = next_column - 1;

        processing_dot = false;

        return STDERR_NONE;
    }

    int c = scanner_getchar();
    State state = STATE_START;
    StringBuffer buffer = {0};

    bool string_has_interpolated_prefix = false;

    while (1) {
        switch (state) {
        // Start
        case STATE_START:
            if (c == EOF) {
                token->type = TOK_EOF;
                return STDERR_NONE;
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
                ScannerErrorCode result = buffer_init(&buffer);
                if (result != SCANNER_OK) {
                    return STDERR_MEMORY_ERROR;
                }

                state = STATE_STRING_FIRST_QUOTE;
                break;
            }

            if (c == '.') {
                ScannerErrorCode result = buffer_init(&buffer);
                if (result != SCANNER_OK) {
                    return STDERR_MEMORY_ERROR;
                }
                result = buffer_append(&buffer, (char)c);
                if (result != SCANNER_OK) {
                    buffer_free(&buffer);
                    return STDERR_MEMORY_ERROR;
                }
                state = STATE_DOT;
                break;
            }

            if (c == '_') {
                ScannerErrorCode result = buffer_init(&buffer);
                if (result != SCANNER_OK) {
                    return STDERR_MEMORY_ERROR;
                }
                result = buffer_append(&buffer, (char)c);
                if (result != SCANNER_OK) {
                    buffer_free(&buffer);
                    return STDERR_MEMORY_ERROR;
                }

                state = STATE_IDENTIFIER_UNDERSCORE;
                break;
            }

            if (isalpha(c)) {
                ScannerErrorCode result = buffer_init(&buffer);
                if (result != SCANNER_OK) {
                    return STDERR_MEMORY_ERROR;
                }
                result = buffer_append(&buffer, (char)c);
                if (result != SCANNER_OK) {
                    buffer_free(&buffer);
                    return STDERR_MEMORY_ERROR;
                }

                state = STATE_IDENTIFIER;
                break;
            }

            if (c == '0') {
                state = STATE_INT_ZERO;
                break;
            }

            if (c >= '1' && c <= '9') {
                ScannerErrorCode result = buffer_init(&buffer);
                if (result != SCANNER_OK) {
                    return STDERR_MEMORY_ERROR;
                }
                result = buffer_append(&buffer, (char)c);
                if (result != SCANNER_OK) {
                    buffer_free(&buffer);
                    return STDERR_MEMORY_ERROR;
                }

                state = STATE_INT_DIGITS;
                break;
            }

            if (c == '}' && current_interpolation.mode != INTERPOLATED_MODE_NONE &&
                current_interpolation.reading_expression == true) {
                token->type = TOK_INTERPOLATION_END;
                current_interpolation.reading_expression = false;
                return STDERR_NONE;
            }

            return STDERR_UNKNOWN_CHARACTER;

        // Identifiers
        case STATE_IDENTIFIER:
            c = scanner_getchar();

            if (buffer.data[0] == 's' && c == '"' && buffer.length == 1) {
                string_has_interpolated_prefix = true;

                buffer.data[0] = '\0';
                buffer.length = 0;

                state = STATE_STRING_FIRST_QUOTE;
                break;
            }

            if (isalnum(c) || (c == '_')) {
                ScannerErrorCode result = buffer_append(&buffer, (char)c);
                if (result != SCANNER_OK) {
                    buffer_free(&buffer);
                    return STDERR_MEMORY_ERROR;
                }

                state = STATE_IDENTIFIER;
                break;
            }

            scanner_ungetc(c);

            token->type = keyword_or_id(buffer.data);
            if (token->type == TOK_ID) {
                token->data.str_val = buffer.data;
            } else {
                buffer_free(&buffer);
            }

            return STDERR_NONE;

        case STATE_IDENTIFIER_UNDERSCORE:
            c = scanner_getchar();
            if (isalnum(c) || (c == '_')) {
                ScannerErrorCode result = buffer_append(&buffer, (char)c);
                if (result != SCANNER_OK) {
                    buffer_free(&buffer);
                    return STDERR_MEMORY_ERROR;
                }

                state = STATE_IDENTIFIER;
                break;
            }

            buffer_free(&buffer);
            return STDERR_INVALID_IDENTIFIER;

        // Integer literals
        case STATE_INT_ZERO: {
            c = scanner_getchar();
            ScannerErrorCode result = buffer_init(&buffer);
            if (result != SCANNER_OK) {
                return STDERR_MEMORY_ERROR;
            }
            result = buffer_append(&buffer, '0');
            if (result != SCANNER_OK) {
                buffer_free(&buffer);
                return STDERR_MEMORY_ERROR;
            }

            if (isdigit(c)) {
                ScannerErrorCode result = buffer_append(&buffer, (char)c);
                if (result != SCANNER_OK) {
                    buffer_free(&buffer);
                    return STDERR_MEMORY_ERROR;
                }
                state = STATE_INT_DIGITS_AFTER_ZERO;
                break;
            }
            if (c == '.') {
                ScannerErrorCode result = buffer_append(&buffer, (char)c);
                if (result != SCANNER_OK) {
                    buffer_free(&buffer);
                    return STDERR_MEMORY_ERROR;
                }
                state = STATE_NUMBER_AFTER_DOT;
                break;
            }
            if (c == 'e' || c == 'E') {
                ScannerErrorCode result = buffer_append(&buffer, (char)c);
                if (result != SCANNER_OK) {
                    buffer_free(&buffer);
                    return STDERR_MEMORY_ERROR;
                }
                state = STATE_DOUBLE_EXPONENT;
                break;
            }
            if (c == '_') {
                state = STATE_INT_UNDERSCORE_AFTER_ZERO;
                break;
            }
            scanner_ungetc(c);

            token->type = TOK_INT_LITERAL;
            token->data.num_int_val = 0;
            buffer_free(&buffer);
            return STDERR_NONE;
        }

        case STATE_INT_DIGITS:
            c = scanner_getchar();
            while (isdigit(c)) {
                ScannerErrorCode result = buffer_append(&buffer, (char)c);
                if (result != SCANNER_OK) {
                    buffer_free(&buffer);
                    return STDERR_MEMORY_ERROR;
                }
                c = scanner_getchar();
            }
            if (c == '_') {
                state = STATE_INT_UNDERSCORE;
                break;
            }
            if (c == '.') {
                ScannerErrorCode result = buffer_append(&buffer, (char)c);
                if (result != SCANNER_OK) {
                    buffer_free(&buffer);
                    return STDERR_MEMORY_ERROR;
                }
                state = STATE_NUMBER_AFTER_DOT;
                break;
            }
            if (c == 'e' || c == 'E') {
                ScannerErrorCode result = buffer_append(&buffer, (char)c);
                if (result != SCANNER_OK) {
                    buffer_free(&buffer);
                    return STDERR_MEMORY_ERROR;
                }
                state = STATE_DOUBLE_EXPONENT;
                break;
            }
            scanner_ungetc(c);
            token->type = TOK_INT_LITERAL;
            token->data.num_int_val = strtoll(buffer.data, NULL, 10);
            buffer_free(&buffer);
            return STDERR_NONE;

        case STATE_INT_UNDERSCORE:
            c = scanner_getchar();
            while (c == '_') {
                c = scanner_getchar();
            }
            if (isdigit(c)) {
                ScannerErrorCode result = buffer_append(&buffer, (char)c);
                if (result != SCANNER_OK) {
                    buffer_free(&buffer);
                    return STDERR_MEMORY_ERROR;
                }

                state = STATE_INT_DIGITS;
                break;
            }
            buffer_free(&buffer);
            return STDERR_INVALID_NUMBER;

        case STATE_INT_DIGITS_AFTER_ZERO:
            c = scanner_getchar();
            while (isdigit(c)) {
                ScannerErrorCode result = buffer_append(&buffer, (char)c);
                if (result != SCANNER_OK) {
                    buffer_free(&buffer);
                    return STDERR_MEMORY_ERROR;
                }

                c = scanner_getchar();
            }
            if (c == 'e' || c == 'E') {
                ScannerErrorCode result = buffer_append(&buffer, (char)c);
                if (result != SCANNER_OK) {
                    buffer_free(&buffer);
                    return STDERR_MEMORY_ERROR;
                }
                state = STATE_DOUBLE_EXPONENT;
                break;
            }
            if (c == '.') {
                ScannerErrorCode result = buffer_append(&buffer, (char)c);
                if (result != SCANNER_OK) {
                    buffer_free(&buffer);
                    return STDERR_MEMORY_ERROR;
                }
                state = STATE_NUMBER_AFTER_DOT;
                break;
            }
            if (c == '_') {
                state = STATE_INT_UNDERSCORE_AFTER_ZERO;
                break;
            }

            for (size_t i = 0; i < buffer.length; i++) {
                if (buffer.data[i] != '0') {
                    buffer_free(&buffer);
                    return STDERR_INVALID_NUMBER;
                }
            }

            scanner_ungetc(c);

            token->type = TOK_INT_LITERAL;
            token->data.num_int_val = 0;
            buffer_free(&buffer);
            return STDERR_NONE;

        case STATE_INT_UNDERSCORE_AFTER_ZERO:
            c = scanner_getchar();
            while (c == '_') {
                c = scanner_getchar();
            }
            if (isdigit(c)) {
                ScannerErrorCode result = buffer_append(&buffer, (char)c);
                if (result != SCANNER_OK) {
                    buffer_free(&buffer);
                    return STDERR_MEMORY_ERROR;
                }
                state = STATE_INT_DIGITS_AFTER_ZERO;
                break;
            }
            buffer_free(&buffer);
            return STDERR_INVALID_NUMBER;

        // Decimal literals
        case STATE_DOT:
            c = scanner_getchar();
            if (isdigit(c)) {
                ScannerErrorCode result = buffer_append(&buffer, (char)c);
                if (result != SCANNER_OK) {
                    buffer_free(&buffer);
                    return STDERR_MEMORY_ERROR;
                }
                state = STATE_DOUBLE_DIGITS;
                break;
            }
            scanner_ungetc(c);

            token->type = TOK_DOT;
            buffer_free(&buffer);
            return STDERR_NONE;

        case STATE_NUMBER_AFTER_DOT:
            c = scanner_getchar();
            if (isdigit(c)) {
                ScannerErrorCode result = buffer_append(&buffer, (char)c);
                if (result != SCANNER_OK) {
                    buffer_free(&buffer);
                    return STDERR_MEMORY_ERROR;
                }
                state = STATE_DOUBLE_DIGITS;
                break;
            }
            if (isalpha(c) || (c == '_')) {
                processing_dot = true;
                
                scanner_ungetc(c);
                buffer.length--;
                buffer.data[buffer.length] = '\0';

                token->type = TOK_INT_LITERAL;
                token->data.num_int_val = strtoll(buffer.data, NULL, 10);
                if (buffer.data[0] == '0' && token->data.num_int_val > 0) {
                    buffer_free(&buffer);
                    return STDERR_INVALID_NUMBER;
                }
                buffer_free(&buffer);
                return STDERR_NONE;
            }
            buffer_free(&buffer);
            return STDERR_INVALID_NUMBER;

        case STATE_DOUBLE_DIGITS:
            c = scanner_getchar();
            while (isdigit(c)) {
                ScannerErrorCode result = buffer_append(&buffer, (char)c);
                if (result != SCANNER_OK) {
                    buffer_free(&buffer);
                    return STDERR_MEMORY_ERROR;
                }

                c = scanner_getchar();
            }
            if (c == '_') {
                state = STATE_DOUBLE_UNDERSCORE;
                break;
            } else if (c == 'D' || c == 'd') {
                state = STATE_DOUBLE_SUFFIX;
                break;
            } else if (c == 'E' || c == 'e') {
                ScannerErrorCode result = buffer_append(&buffer, (char)c);
                if (result != SCANNER_OK) {
                    buffer_free(&buffer);
                    return STDERR_MEMORY_ERROR;
                }

                state = STATE_DOUBLE_EXPONENT;
                break;
            }
            scanner_ungetc(c);

            token->type = TOK_DOUBLE_LITERAL;
            token->data.num_float_val = strtod(buffer.data, NULL);

            buffer_free(&buffer);
            return STDERR_NONE;

        case STATE_DOUBLE_UNDERSCORE:
            c = scanner_getchar();
            while (c == '_') {
                c = scanner_getchar();
            }
            if (isdigit(c)) {
                ScannerErrorCode result = buffer_append(&buffer, (char)c);
                if (result != SCANNER_OK) {
                    buffer_free(&buffer);
                    return STDERR_MEMORY_ERROR;
                }

                state = STATE_DOUBLE_DIGITS;
                break;
            }

            buffer_free(&buffer);
            return STDERR_INVALID_NUMBER;

        case STATE_DOUBLE_EXPONENT:
            c = scanner_getchar();
            if (c == '+' || c == '-') {
                ScannerErrorCode result = buffer_append(&buffer, (char)c);
                if (result != SCANNER_OK) {
                    buffer_free(&buffer);
                    return STDERR_MEMORY_ERROR;
                }

                state = STATE_DOUBLE_EXPONENT_SIGN;
                break;
            } else if (isdigit(c)) {
                ScannerErrorCode result = buffer_append(&buffer, (char)c);
                if (result != SCANNER_OK) {
                    buffer_free(&buffer);
                    return STDERR_MEMORY_ERROR;
                }
                state = STATE_DOUBLE_EXPONENT_DIGITS;
                break;
            }

            buffer_free(&buffer);
            return STDERR_INVALID_NUMBER;

        case STATE_DOUBLE_EXPONENT_SIGN:
            c = scanner_getchar();
            if (isdigit(c)) {
                ScannerErrorCode result = buffer_append(&buffer, (char)c);
                if (result != SCANNER_OK) {
                    buffer_free(&buffer);
                    return STDERR_MEMORY_ERROR;
                }

                state = STATE_DOUBLE_EXPONENT_DIGITS;
                break;
            }
            buffer_free(&buffer);
            return STDERR_INVALID_NUMBER;

        case STATE_DOUBLE_EXPONENT_DIGITS:
            c = scanner_getchar();
            while (isdigit(c)) {
                ScannerErrorCode result = buffer_append(&buffer, (char)c);
                if (result != SCANNER_OK) {
                    buffer_free(&buffer);
                    return STDERR_MEMORY_ERROR;
                }

                c = scanner_getchar();
            }
            if (c == '_') {
                state = STATE_DOUBLE_EXPONENT_UNDERSCORE;
                break;
            } else if (c == 'D' || c == 'd') {
                state = STATE_DOUBLE_SUFFIX;
                break;
            }
            scanner_ungetc(c);

            token->type = TOK_DOUBLE_LITERAL;
            token->data.num_float_val = strtod(buffer.data, NULL);

            buffer_free(&buffer);
            return STDERR_NONE;

        case STATE_DOUBLE_EXPONENT_UNDERSCORE:
            c = scanner_getchar();
            while (c == '_') {
                c = scanner_getchar();
            }
            if (isdigit(c)) {
                ScannerErrorCode result = buffer_append(&buffer, (char)c);
                if (result != SCANNER_OK) {
                    buffer_free(&buffer);
                    return STDERR_MEMORY_ERROR;
                }

                state = STATE_DOUBLE_EXPONENT_DIGITS;
                break;
            }
            buffer_free(&buffer);
            return STDERR_INVALID_NUMBER;

        case STATE_DOUBLE_SUFFIX:
            token->type = TOK_DOUBLE_LITERAL;
            token->data.num_float_val = strtod(buffer.data, NULL);
            buffer_free(&buffer);
            return STDERR_NONE;

        // String literals
        case STATE_STRING_FIRST_QUOTE:
            c = scanner_getchar();
            if (c == '"') {
                state = STATE_STRING_SECOND_QUOTE;
                break;
            }

            if (string_has_interpolated_prefix == true) {
                scanner_ungetc(c);

                StderrMessage result = interpolation_push();
                if (result != STDERR_NONE) {
                    buffer_free(&buffer);
                    return result;
                }

                current_interpolation.mode = INTERPOLATED_MODE_SINGLE_LINE;
                current_interpolation.reading_expression = false;
                token->type = TOK_INTERPOLATED_BEGIN;

                buffer_free(&buffer);
                return STDERR_NONE;
            }

            while (c != '"') {
                if (c == EOF || c == '\n') {
                    buffer_free(&buffer);
                    return STDERR_OPEN_STRING;
                }
                if (c == '\\') {
                    state = STATE_STRING_ESCAPE;
                    break;
                }

                ScannerErrorCode result = buffer_append(&buffer, (char)c);
                if (result != SCANNER_OK) {
                    buffer_free(&buffer);
                    return STDERR_MEMORY_ERROR;
                }

                c = scanner_getchar();
            }
            if (state == STATE_STRING_ESCAPE) {
                break;
            }

            token->type = TOK_STRING_LITERAL;
            token->data.str_val = buffer.data;
            return STDERR_NONE;

        case STATE_STRING_SECOND_QUOTE:
            c = scanner_getchar();
            if (c == '"') {
                if (string_has_interpolated_prefix == true) {
                    StderrMessage result = interpolation_push();
                    if (result != STDERR_NONE) {
                        buffer_free(&buffer);
                        return result;
                    }

                    current_interpolation.mode = INTERPOLATED_MODE_MULTILINE;
                    current_interpolation.reading_expression = false;
                    token->type = TOK_INTERPOLATED_BEGIN;

                    buffer_free(&buffer);
                    return STDERR_NONE;
                }
                state = STATE_STRING_MULTILINE_BODY;
                break;
            }

            scanner_ungetc(c);
            token->type = TOK_STRING_LITERAL;
            token->data.str_val = buffer.data;
            return STDERR_NONE;

        case STATE_STRING_BODY:
            c = scanner_getchar();
            while (c != '"') {
                if (c == EOF || c == '\n') {
                    buffer_free(&buffer);
                    return STDERR_OPEN_STRING;
                }
                if (c == '\\') {
                    state = STATE_STRING_ESCAPE;
                    break;
                }

                ScannerErrorCode result = buffer_append(&buffer, (char)c);
                if (result != SCANNER_OK) {
                    buffer_free(&buffer);
                    return STDERR_MEMORY_ERROR;
                }

                c = scanner_getchar();
            }
            if (state == STATE_STRING_ESCAPE) {
                break;
            }

            token->type = TOK_STRING_LITERAL;
            token->data.str_val = buffer.data;
            return STDERR_NONE;

        case STATE_STRING_ESCAPE:
            c = scanner_getchar();
            if (c == '"' || c == '\\') {
                ScannerErrorCode result = buffer_append(&buffer, (char)c);
                if (result != SCANNER_OK) {
                    buffer_free(&buffer);
                    return STDERR_MEMORY_ERROR;
                }

                state = STATE_STRING_BODY;
                break;
            } else if (c == 'n') {
                c = '\n';
                ScannerErrorCode result = buffer_append(&buffer, (char)c);
                if (result != SCANNER_OK) {
                    buffer_free(&buffer);
                    return STDERR_MEMORY_ERROR;
                }
                state = STATE_STRING_BODY;
                break;
            } else if (c == 't') {
                c = '\t';
                ScannerErrorCode result = buffer_append(&buffer, (char)c);
                if (result != SCANNER_OK) {
                    buffer_free(&buffer);
                    return STDERR_MEMORY_ERROR;
                }
                state = STATE_STRING_BODY;
                break;
            } else if (c == 'r') {
                c = '\r';
                ScannerErrorCode result = buffer_append(&buffer, (char)c);
                if (result != SCANNER_OK) {
                    buffer_free(&buffer);
                    return STDERR_MEMORY_ERROR;
                }
                state = STATE_STRING_BODY;
                break;
            }
            buffer_free(&buffer);
            return STDERR_INVALID_ESCAPE;

        case STATE_STRING_MULTILINE_BODY:
            c = scanner_getchar();
            while (c != '"') {
                if (c == EOF) {
                    buffer_free(&buffer);
                    return STDERR_OPEN_STRING;
                }

                ScannerErrorCode result = buffer_append(&buffer, (char)c);
                if (result != SCANNER_OK) {
                    buffer_free(&buffer);
                    return STDERR_MEMORY_ERROR;
                }

                c = scanner_getchar();
            }
            state = STATE_STRING_MULTILINE_FIRST_QUOTE;
            break;

        case STATE_STRING_MULTILINE_FIRST_QUOTE: {
            c = scanner_getchar();
            if (c == EOF) {
                buffer_free(&buffer);
                return STDERR_OPEN_STRING;
            }
            if (c == '"') {
                state = STATE_STRING_MULTILINE_SECOND_QUOTE;
                break;
            }
            scanner_ungetc(c);

            ScannerErrorCode result = buffer_append(&buffer, '"');
            if (result != SCANNER_OK) {
                buffer_free(&buffer);
                return STDERR_MEMORY_ERROR;
            }

            state = STATE_STRING_MULTILINE_BODY;
            break;
        }

        case STATE_STRING_MULTILINE_SECOND_QUOTE: {
            c = scanner_getchar();
            if (c == EOF) {
                buffer_free(&buffer);
                return STDERR_OPEN_STRING;
            }
            if (c == '"') {
                state = STATE_STRING_MULTILINE_CLOSING_QUOTES;
                break;
            }

            ScannerErrorCode result = buffer_append(&buffer, '"');
            if (result != SCANNER_OK) {
                buffer_free(&buffer);
                return STDERR_MEMORY_ERROR;
            }

            result = buffer_append(&buffer, '"');
            if (result != SCANNER_OK) {
                buffer_free(&buffer);
                return STDERR_MEMORY_ERROR;
            }

            scanner_ungetc(c);

            state = STATE_STRING_MULTILINE_BODY;
            break;
        }

        case STATE_STRING_MULTILINE_CLOSING_QUOTES:
            c = scanner_getchar();
            while (c == '"') {
                ScannerErrorCode result = buffer_append(&buffer, (char)c);
                if (result != SCANNER_OK) {
                    buffer_free(&buffer);
                    return STDERR_MEMORY_ERROR;
                }
                c = scanner_getchar();
            }

            scanner_ungetc(c);

            token->type = TOK_STRING_LITERAL;
            token->data.str_val = buffer.data;
            return STDERR_NONE;

        // Operators and separators
        case STATE_PLUS:
            token->type = TOK_PLUS;
            return STDERR_NONE;

        case STATE_MINUS:
            token->type = TOK_MINUS;
            return STDERR_NONE;

        case STATE_MULTIPLY:
            token->type = TOK_MULTIPLY;
            return STDERR_NONE;

        case STATE_SLASH:
            c = scanner_getchar();
            if (c == '/') {
                state = STATE_ONE_LINE_COMMENT;
                break;
            }
            if (c == '*') {
                state = STATE_MULTI_LINE_COMMENT;
                reading_indentation = false;
                comments_depth = 1;
                comment_has_lf = false;
                break;
            }

            scanner_ungetc(c);

            token->type = TOK_DIVISION;
            return STDERR_NONE;

        case STATE_LESS:
            c = scanner_getchar();
            if (c == '=') {
                token->type = TOK_LESS_OR_EQUAL_THAN;
                return STDERR_NONE;
            }

            if (c == '-') {
                token->type = TOK_CONTROL_ARROW;
                return STDERR_NONE;
            }

            scanner_ungetc(c);

            token->type = TOK_LESS_THAN;
            return STDERR_NONE;

        case STATE_GREATER:
            c = scanner_getchar();
            if (c == '=') {
                token->type = TOK_GREATER_OR_EQUAL_THAN;
                return STDERR_NONE;
            }

            scanner_ungetc(c);

            token->type = TOK_GREATER_THAN;
            return STDERR_NONE;

        case STATE_EQUAL_SIGN:
            c = scanner_getchar();
            if (c == '=') {
                token->type = TOK_EQUAL;
                return STDERR_NONE;
            }

            scanner_ungetc(c);

            token->type = TOK_ASSIGN;
            return STDERR_NONE;

        case STATE_EXCLAMATION_MARK:
            c = scanner_getchar();
            if (c == '=') {
                token->type = TOK_NOT_EQUAL;
                return STDERR_NONE;
            }

            scanner_ungetc(c);

            token->type = TOK_NOT;
            return STDERR_NONE;

        case STATE_AMPERSAND:
            c = scanner_getchar();
            if (c == '&') {
                token->type = TOK_AND;
                return STDERR_NONE;
            }

            return STDERR_UNKNOWN_CHARACTER;

        case STATE_VERTICAL_BAR:
            c = scanner_getchar();
            if (c == '|') {
                token->type = TOK_OR;
                return STDERR_NONE;
            }
            return STDERR_UNKNOWN_CHARACTER;

        case STATE_LEFT_PARENTHESES:
            token->type = TOK_LEFT_PARENTHESIS;
            parentheses_depth++;
            return STDERR_NONE;

        case STATE_RIGHT_PARENTHESES:
            token->type = TOK_RIGHT_PARENTHESIS;
            if (parentheses_depth > 0) {
                parentheses_depth--;
            }
            return STDERR_NONE;

        case STATE_COMMA:
            token->type = TOK_COMMA;
            return STDERR_NONE;

        case STATE_COLON:
            token->type = TOK_COLON;
            return STDERR_NONE;

        case STATE_SEMICOLON:
            token->type = TOK_SEMICOLON;
            return STDERR_NONE;

        case STATE_AT:
            token->type = TOK_AT;
            return STDERR_NONE;

        // Comments
        case STATE_ONE_LINE_COMMENT:
            c = scanner_getchar();
            while (c != '\n' && c != EOF) {
                c = scanner_getchar();
            }

            if (c == EOF) {
                token->type = TOK_EOF;
                return STDERR_NONE;
            }

            state = STATE_NEW_LINE;
            break;

        case STATE_MULTI_LINE_COMMENT:
            c = scanner_getchar();
            while (1) {
                while (c != '*' && c != EOF && c != '/' && c != '\n') {
                    c = scanner_getchar();
                }
                if (c == EOF) {
                    return STDERR_OPEN_COMMENT;
                }
                if (c == '\n') {
                    comment_has_lf = true;
                    c = scanner_getchar();
                    continue;
                }
                if (c == '/') {
                    c = scanner_getchar();
                    if (c == EOF) {
                        return STDERR_OPEN_COMMENT;
                    }
                    if (c == '*') {
                        comments_depth++;
                        c = scanner_getchar();
                        continue;
                    }
                }
                if (c == '*') {
                    c = scanner_getchar();
                    if (c == EOF) {
                        return STDERR_OPEN_COMMENT;
                    }
                    if (c == '/') {
                        comments_depth--;
                        if (comments_depth == 0) {
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
                    return STDERR_NONE;
                }
                return STDERR_TOKEN_AFTER_COMMENT;
            }
            if (c == EOF) {
                token->type = TOK_EOF;
                return STDERR_NONE;
            }

            state = STATE_START;
            break;

        // End of line
        case STATE_NEW_LINE:
            reading_indentation = true;
            current_indent = 0;
            tab_in_indent = false;
            trailing_comma_allowed = true;

            if (parentheses_depth == 0 && line_has_token) {
                token->type = TOK_EOL;
                line_has_token = false;
                return STDERR_NONE;
            }

            c = scanner_getchar();
            if (c == EOF) {
                token->type = TOK_EOF;
                return STDERR_NONE;
            }

            state = STATE_START;
            break;

        default:
            buffer_free(&buffer);
            return STDERR_INVALID_STATE;
        }
    }
}


ScannerErrorCode scanner_get_token(Token *token) {
    token->indent = 0;
    token->lf_before = false;

    StderrMessage result;
    if (current_interpolation.mode == INTERPOLATED_MODE_NONE || current_interpolation.reading_expression == true) {
        result = scanner_read_token(token);
    } else {
        result = scanner_read_interpolated_text(token);
    }

    if (ferror(stdin) != 0) {
        if (result == STDERR_NONE && (token->type == TOK_ID || token->type == TOK_STRING_LITERAL)) {
            free(token->data.str_val);
            token->data.str_val = NULL;
        }
        interpolation_free();
        return scanner_error_message(STDERR_STDIN_ERROR);
    }

    if (result != STDERR_NONE) {
        interpolation_free();
        return scanner_error_message(result);
    }

    if (token->type == TOK_EOF && current_interpolation.reading_expression == true) {
        interpolation_free();
        return scanner_error_message(STDERR_OPEN_INTERPOLATION);
    }

    if (token->type == TOK_EOL || token->type == TOK_EOF) {
        token->line = last_line;
        token->column = last_column;
        return SCANNER_OK;
    }

    if (tab_in_indent == true) {
        if (token->type == TOK_ID || token->type == TOK_STRING_LITERAL) {
            free(token->data.str_val);
            token->data.str_val = NULL;
        }
        interpolation_free();
        return scanner_error_message(STDERR_TAB_IN_INDENT);
    }

    token->lf_before = trailing_comma_allowed;
    trailing_comma_allowed = false;

    line_has_token = true;
    reading_indentation = false;
    token->indent = current_indent;

    return SCANNER_OK;
}
