/**
 * @file hashtable.h
 * @brief Header file for Hashtable ADT
 * @author Filip Radulovič (xradulf00)
 */

#ifndef HASHTABLE_H
#define HASHTABLE_H

#include <stdbool.h>

typedef struct Htab Htab;

typedef const void* HtabKey;
typedef void* HtabValue;

typedef unsigned long HtabSize;

typedef HtabSize (*HtabHashFn)(HtabKey key);
typedef bool (*HtabEqFn)(HtabKey a, HtabKey b);

// this can be NULL, depends on who owns the values (caller or the table).
typedef void (*HtabFreeFn)(void* ptr);

typedef enum {
    HTAB_OK, // inserted sucessfully.
    HTAB_EXISTS, // key already exists, nothing changed.
    HTAB_NOMEM // table grow failed, nothing changed.
} HtabRes;

// creates a table. initial capacity below 8 is raised to 8.
// hash and eq fns cannot be null, key_free and value_free can.
// returns NULL if the table could not be created.
Htab* htab_init(
    HtabSize initial_capacity, HtabHashFn hash, HtabEqFn eq, HtabFreeFn key_free, HtabFreeFn value_free);

// destroys the table.
// frees every value and key if key_free and value_free fns were provided.
void htab_destroy(Htab* ht);

// inserts slot into table. ht cannot be NULL.
HtabRes htab_insert(Htab* ht, HtabKey key, HtabValue value);

// searchs for a key. returns true and stores value in *out if key was found.
// if key was not found, returns false and *out is unused.
// ht cannot be NULL.
bool htab_get(const Htab* ht, HtabKey key, HtabValue* out);

// removes key and calls freeing functions (if set).
// return true if succesfull, otherwise false.
// ht cannot be NULL
bool htab_remove(Htab* ht, HtabKey key);

// returns the size of table (number of entries).
// ht cannot be NULL.
HtabSize htab_size(const Htab* ht);

#endif // HASHTABLE_H
