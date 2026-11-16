/**
 * @file parser.c
 * @brief Syntax analyser
 * @author Oleksandr Teslja (xtesljo00)
 */

#include "parser.h"
#include "scanner.h"
#include <stdlib.h>
#include <string.h>

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

    if (p->current.type == TOK_EOF) {
        // EOL token needs to be filled (if missing) before closing out the file with EOF
        if (!p->at_line_start) {
            p->lookahead = p->current;
            p->has_lookahead = true;
            p->current = (Token){.type = TOK_EOL, .line = p->lookahead.line};
            p->at_line_start = true;
            return PARSE_OK;
        }

        // setting indent lvl to 0 for EOF will cause the parser to fill the remaining DEDENTs
        p->current.indent = 0;
    }

    if (!p->at_line_start) {
        return PARSE_OK;
    }

    // compare indentation level of the current token with the top of the stack
    unsigned int top = p->indent_stack[p->indent_count - 1];

    if (p->current.indent > top) {
        p->at_line_start = false; // INDENTs only come one at a time, so we toggle this off

        ParseResult result = indent_push(p, p->current.indent);
        if (result != PARSE_OK) {
            return result;
        }

        // store the real token as lookahead and return an INDENT token
        p->lookahead = p->current;
        p->has_lookahead = true;

        p->current = (Token){.type = TOK_INDENT, .line = p->lookahead.line, .indent = p->lookahead.indent};
        return PARSE_OK;
    }

    if (p->current.indent < top) {
        // DEDENTs may come more than one at a time, so we need to handle it with a counter
        p->indent_count--;

        // indentation between two open blocks
        if (p->indent_stack[p->indent_count - 1] < p->current.indent) {
            return PARSE_LEXICAL_ERROR;
        }

        // store the real token as lookahead and return a DEDENT token
        p->lookahead = p->current;
        p->has_lookahead = true;

        p->current = (Token){.type = TOK_DEDENT, .line = p->lookahead.line, .indent = p->lookahead.indent};
        return PARSE_OK;
    }

    // same indentation as the current block
    p->at_line_start = false;
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

// checks that the current token has the given type and moves to the next one
static ParseResult expect(Parser *p, TokenType type) {
    if (p->current.type != type) {
        return PARSE_SYNTAX_ERROR;
    }
    return next_token(p);
}

// <functions> -> <function> <functions> | <function_main> <functions> | epsilon
static ParseResult parse_functions(Parser *p, AstNodeList *functions) {
    (void)p;
    (void)functions;
    return PARSE_OK;
}

// <program> -> TOK_KW_IMPORT TOK_ID TOK_DOT TOK_MULTIPLY TOK_EOL <functions> TOK_EOF
ParseResult parse_program(Parser *p, AstNode **out) {
    ParseResult result;
    result = next_token(p);
    if (result != PARSE_OK) {
        return result;
    }

    result = expect(p, TOK_KW_IMPORT);
    if (result != PARSE_OK) {
        return result;
    }
    if (p->current.type != TOK_ID) {
        return PARSE_SYNTAX_ERROR;
    }
    if (strcmp(p->current.data.str_val, "ifj26") != 0) {
        return PARSE_SYNTAX_ERROR;
    }

    result = next_token(p);
    if (result != PARSE_OK) {
        return result;
    }

    result = expect(p, TOK_DOT);
    if (result != PARSE_OK) {
        return result;
    }

    result = expect(p, TOK_MULTIPLY);
    if (result != PARSE_OK) {
        return result;
    }

    result = expect(p, TOK_EOL);
    if (result != PARSE_OK) {
        return result;
    }

    AstNode *node = ast_new(AST_PROGRAM, p->current.line);
    if (node == NULL) {
        return PARSE_INTERNAL_ERROR;
    }

    result = parse_functions(p, &node->as.program.functions);
    if (result != PARSE_OK) {
        ast_free(node);
        return result;
    }

    if (p->current.type != TOK_EOF) {
        ast_free(node);
        return PARSE_SYNTAX_ERROR;
    }

    *out = node;
    return PARSE_OK;
}