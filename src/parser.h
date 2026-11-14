/**
 * @file parser.h
 * @brief Syntax analyser
 * @author Oleksandr Teslja (xtesljo00)
 */

#ifndef PARSER_H
#define PARSER_H

#include "token.h"
#include <stdbool.h>

typedef enum ParseResult {
    PARSE_OK = 0,
    PARSE_LEXICAL_ERROR = 1,
    PARSE_SYNTAX_ERROR = 2,
    PARSE_INTERNAL_ERROR = 99,
} ParseResult;

typedef struct Parser {
    Token current;
    Token lookahead;    // token after the current one
    bool has_lookahead;
} Parser;

// moves p->current to the next token received from the scanner
// also frees the string of the previous token, so if you need to keep it:
//  1. copy the pointer
//  2. set p->current.data.str_val to NULL
//  3. only then call next_token(p)
ParseResult next_token(Parser *p);

// stores the type of the token after p->current into *type without moving the parser to it
ParseResult lookahead_token(Parser *p, TokenType *type);

#endif
