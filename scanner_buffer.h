#ifndef SCANNER_BUFFER_H
#define SCANNER_BUFFER_H

#include <stddef.h>
#include "scanner.h"

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} StringBuffer;

ScannerErrorCode buffer_init(StringBuffer *buffer);
void buffer_free(StringBuffer *buffer);
ScannerErrorCode buffer_increase(StringBuffer *buffer);
ScannerErrorCode buffer_append(StringBuffer *buffer, char c);


#endif
