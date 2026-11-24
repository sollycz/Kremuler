/**
 * @file parser.h
 * @brief Syntax analyser
 * @author Oleksandr Teslja (xtesljo00)
 */

#ifndef PARSER_H
#define PARSER_H

#include "ast.h"
#include "token.h"
#include <stdbool.h>
#include <stddef.h>

typedef enum ParseResult {
    PARSE_OK = 0,
    PARSE_LEXICAL_ERROR = 1,
    PARSE_SYNTAX_ERROR = 2,
    PARSE_INTERNAL_ERROR = 99,
} ParseResult;

typedef struct Parser {
    Token current;
    Token lookahead; // token after the current one
    bool has_lookahead;
    unsigned int *indent_stack; // for keeping track of indentation [0] -> [0, 4] -> ...
    size_t indent_count;
    bool at_line_start;      // (is true after TOK_EOL or at the start of the file)
    TokenType previous_type; // type of the token before current (CONTROL)
} Parser;

// initializes the parser
// (PARSE_INTERNAL_ERROR if memory allocation fails)
ParseResult parser_init(Parser *p);

// frees the resources used by the parser
void parser_destroy(Parser *p);

// moves p->current to the next token received from the scanner
// also frees the string of the previous token, so if you need to keep it:
//  1. copy the pointer
//  2. set p->current.data.str_val to NULL
//  3. only then call next_token(p)
ParseResult next_token(Parser *p);

// stores the type of the token after p->current into *type without moving the parser to it
// (also must not be called when current token is TOK_EOL, since that would skip the indentation of the next line)
ParseResult lookahead_token(Parser *p, TokenType *type);

// parses the whole input file and stores the root AST node into 'out'
ParseResult parse_program(Parser *p, AstNode **out);

#endif
