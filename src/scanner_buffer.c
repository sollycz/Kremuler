/**
 * @file scanner_buffer.c
 * @brief Dynamic character buffer implementation for the IFJ26 scanner.
 * @author David Zoblivý (xzoblid00)
 * @date 14.11.2026
 */

#include "scanner_buffer.h"
#include <stdlib.h>
#include <stdint.h>

#define INIT_CAPACITY 16

/**
 * @brief initialize dynamic memory buffer
 * @param[out] buffer to initialize
 * @return SCANNER_OK, otherwise SCANNER_MEMORY_ERROR
 * @note the buffer is initially empty and terminated with '\0'
 */
ScannerErrorCode buffer_init(StringBuffer *buffer)
{
    buffer->data = malloc(INIT_CAPACITY);
    if (buffer->data == NULL) {
        return SCANNER_MEMORY_ERROR;
    }

    buffer->length = 0;
    buffer->capacity = INIT_CAPACITY;
    buffer->data[0] = '\0';

    return SCANNER_OK;
}

/**
 * @brief free dynamic memory buffer
 * @param[in, out] buffer whose memory is freed and fields are reset
 */
void buffer_free(StringBuffer *buffer)
{
    free(buffer->data);

    buffer->data = NULL;
    buffer->length = 0;
    buffer->capacity = 0;
}

/**
 * @brief buffer capacity will be doubled
 * @param[in, out] buffer whose capacity is doubled
 * @return SCANNER_OK on success, otherwise SCANNER_MEMORY_ERROR
 */
ScannerErrorCode buffer_increase(StringBuffer *buffer)
{
    if (buffer->capacity > SIZE_MAX / 2) {
        return SCANNER_MEMORY_ERROR;
    }

    size_t new_capacity = buffer->capacity * 2;

    char *new_data = realloc(buffer->data, new_capacity);
    if (new_data == NULL) {
        return SCANNER_MEMORY_ERROR;
    }

    buffer->data = new_data;
    buffer->capacity = new_capacity;

    return SCANNER_OK;
}

/**
 * @brief character will be added at the end of the buffer
 * @param[in, out] buffer to which the character is appended
 * @param[in] c character that will be added
 * @return SCANNER_OK on success, otherwise SCANNER_MEMORY_ERROR
 */
ScannerErrorCode buffer_append(StringBuffer *buffer, char c)
{
    if (buffer->length + 2 > buffer->capacity) {
        ScannerErrorCode result = buffer_increase(buffer);
        if (result != SCANNER_OK) {
            return result;
        }
    }

    buffer->data[buffer->length] = c;
    buffer->length++;
    buffer->data[buffer->length] = '\0';

    return SCANNER_OK;
}
