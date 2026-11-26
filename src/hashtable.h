/**
 * @file hashtable.h
 * @brief Header file for Hashtable ADT
 * @author Filip Radulovič (xradulf00)
 */

#ifndef HASHTABLE_H
#define HASHTABLE_H

#include <stdbool.h>

typedef struct htab htab;

typedef const void* htab_key_t;
typedef void* htab_value_t;

typedef unsigned long htab_size_t;

typedef htab_size_t (*htab_hash_fn)(htab_key_t key);
typedef bool (*htab_eq_fn)(htab_key_t a, htab_key_t b);

// this can be NULL, depends on who owns the values (caller or the table).
typedef void (*htab_free_fn)(void* ptr);

typedef enum {
    HTAB_OK, // inserted sucessfully.
    HTAB_EXISTS, // key already exists, nothing changed.
    HTAB_NOMEM // table grow failed, nothing changed.
} htab_res_t;

// creates a table. initial capacity below 8 is raised to 8.
// hash and eq fns cannot be null, key_free and value_free can.
// returns NULL if the table could not be created.
htab* htab_init(
    htab_size_t initial_capacity, htab_hash_fn hash, htab_eq_fn eq, htab_free_fn key_free, htab_free_fn value_free);

// destroys the table.
// frees every value and key if key_free and value_free fns were provided.
void htab_destroy(htab* ht);

// inserts slot into table. ht cannot be NULL.
htab_res_t htab_insert(htab* ht, htab_key_t key, htab_value_t value);

// searchs for a key. returns true and stores value in *out if key was found.
// if key was not found, returns false and *out is unused.
// ht cannot be NULL.
bool htab_get(const htab* ht, htab_key_t key, htab_value_t* out);

// removes key and calls freeing functions (if set).
// return true if succesfull, otherwise false.
// ht cannot be NULL
bool htab_remove(htab* ht, htab_key_t key);

// returns the size of table (number of entries).
// ht cannot be NULL.
htab_size_t htab_size(const htab* ht);

#endif // HASHTABLE_H
