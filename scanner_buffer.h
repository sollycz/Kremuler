#ifndef SCANNER_BUFFER_H
#define SCANNER_BUFFER_H

#include <stddef.h>

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} StringBuffer;

int buffer_init(StringBuffer *buffer);
void buffer_free(StringBuffer *buffer);
int buffer_increase(StringBuffer *buffer);
int buffer_append(StringBuffer *buffer, char c);


#endif