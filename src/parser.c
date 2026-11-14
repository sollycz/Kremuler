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
        return PARSE_OK;
    }

    return (ParseResult)scanner_get_token(&p->current);
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
