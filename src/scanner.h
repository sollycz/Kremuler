/**
 * @file scanner.h
 * @brief Header file for the lexical analyzer for IFJ26
 * @author David Zoblivý (xzoblid00)
 * @date 14.11.2026
 */

#ifndef SCANNER_H
#define SCANNER_H

#include "token.h"

typedef enum {
    SCANNER_OK = 0,
    SCANNER_LEXICAL_ERROR = 1,
    SCANNER_MEMORY_ERROR = 99,
    SCANNER_INVALID_STATE_ERROR = 99,
    SCANNER_READ_ERROR = 99
} ScannerErrorCode;

/** 
 * @brief Gets token from stdin
 * @param[out] token with valid token->type and token->data and token->indent
 * @return SCANNER_OK on success, otherwise a scanner error code
 * @note TOK_ID and TOK_STRING_LITERAL -> caller must free memory token->data.str_val
*/
ScannerErrorCode scanner_get_token(Token *token);

#endif
