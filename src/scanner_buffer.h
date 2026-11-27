/**
 * @file scanner_buffer.h
 * @brief Header file for the dynamic character buffer of the IFJ26 scanner.
 * @author David Zoblivý (xzoblid00)
 * @todo add comments
 */

#ifndef SCANNER_BUFFER_H
#define SCANNER_BUFFER_H

#include "scanner.h"
#include <stddef.h>

typedef struct {
    char* data;
    size_t length;
    size_t capacity;
} StringBuffer;

ScannerErrorCode buffer_init(StringBuffer* buffer);
void buffer_free(StringBuffer* buffer);
ScannerErrorCode buffer_increase(StringBuffer* buffer);
ScannerErrorCode buffer_append(StringBuffer* buffer, char c);

#endif
