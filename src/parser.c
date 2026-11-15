/**
 * @file parser.c
 * @brief Syntax analyser
 * @author Oleksandr Teslja (xtesljo00)
 */

#include "parser.h"
#include "scanner.h"
#include <stdlib.h>

// frees the string of a token if it still has one
static void token_free(Token *token) {
    if (token->type == TOK_ID || token->type == TOK_STRING_LITERAL) {
        free(token->data.str_val);
        token->data.str_val = NULL;
    }
}

// adds an indentation level onto the top of the stack
static ParseResult indent_push(Parser *p, unsigned int indent) {
    // add room for one more indent level
    unsigned int *stack = realloc(p->indent_stack, (p->indent_count + 1) * sizeof(unsigned int));

    if (stack == NULL) {
        return PARSE_INTERNAL_ERROR;
    }

    stack[p->indent_count] = indent;
    p->indent_stack = stack;
    p->indent_count++;

    return PARSE_OK;
}

ParseResult parser_init(Parser *p) {
    *p = (Parser){0};
    p->at_line_start = true;
    return indent_push(p, 0);
}

void parser_destroy(Parser *p) {
    token_free(&p->current);

    if (p->has_lookahead) {
        token_free(&p->lookahead);
    }

    free(p->indent_stack);
    *p = (Parser){0};
}

ParseResult next_token(Parser *p) {
    token_free(&p->current);

    // if we have a stored lookahead token, use that instead of asking the scanner for a new one
    if (p->has_lookahead) {
        p->current = p->lookahead;
        p->has_lookahead = false;
    } else {
        ParseResult result = (ParseResult)scanner_get_token(&p->current);
        if (result != PARSE_OK) {
            return result;
        }
    }

    if (p->current.type == TOK_EOL) {
        p->at_line_start = true;
        return PARSE_OK;
    }

    if (!p->at_line_start || p->current.type == TOK_EOF) {
        return PARSE_OK;
    }

    p->at_line_start = false;

    // compare indentation level of the current token with the top of the stack
    unsigned int top = p->indent_stack[p->indent_count - 1];
    if (p->current.indent > top) {
        ParseResult result = indent_push(p, p->current.indent);
        if (result != PARSE_OK) {
            return result;
        }

        // store the real token as lookahead and return an INDENT token
        p->lookahead = p->current;
        p->has_lookahead = true;

        p->current = (Token){.type = TOK_INDENT, .line = p->lookahead.line, .indent = p->lookahead.indent};
    }

    return PARSE_OK;
}

ParseResult lookahead_token(Parser *p, TokenType *type) {
    if (!p->has_lookahead) {
        ParseResult result = (ParseResult)scanner_get_token(&p->lookahead);

        if (result != PARSE_OK) {
            return result;
        }

        p->has_lookahead = true;
    }

    *type = p->lookahead.type;
    return PARSE_OK;
}
