#ifndef HASHTABLE_H
#define HASHTABLE_H

#include <stdbool.h>

typedef struct htab htab;

typedef const void *htab_key_t;
typedef void *htab_value_t;

typedef unsigned long htab_size_t;

typedef htab_size_t (*htab_hash_fn)(htab_key_t key);
typedef bool (*htab_eq_fn)(htab_key_t a, htab_key_t b);
typedef void (*htab_foreach_fn)(htab_key_t key, htab_value_t value);

// this can be NULL, depends on who owns the values (who is responsible for freeing them)
typedef void (*htab_free_fn)(void *ptr);

typedef enum { HTAB_OK, HTAB_EXISTS, HTAB_NOMEM } htab_res_t;

// creates table, returns NULL if failed
htab *htab_init(htab_size_t initial_capacity, htab_hash_fn hash, htab_eq_fn eq, htab_free_fn key_free, htab_free_fn value_free);

// destroys table and removes all slots, if key_free and value_free functions were provided
void htab_destroy(htab *ht);

// inserts slot into table, returns enum
htab_res_t htab_insert(htab *ht, htab_key_t key, htab_value_t value);

// returns false if key was not found, otherwise returns false and set out to the coresponding value
// will return false if no htab is provided
bool htab_get(const htab *ht, htab_key_t key, htab_value_t *out);

// removes slot and calls freeing functions (if set)
// return true if succesfull, otherwise false
bool htab_remove(htab *ht, htab_key_t key);

// returns the size of table
htab_size_t htab_size(const htab *ht);

// runs foreach_fn for every used slot
void htab_foreach(const htab *ht, htab_foreach_fn fn);

#endif // HASHTABLE_H
