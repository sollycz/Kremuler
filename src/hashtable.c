#include "hashtable.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>

// starting size of table
#define HTAB_MIN_CAPACITY 8

typedef enum { SLOT_EMPTY, SLOT_USED, SLOT_DELETED } slot_state_t;

typedef struct {
    slot_state_t state;
    htab_size_t hash;
    htab_key_t key;
    htab_value_t value;
} htab_slot_t;

struct htab {
    htab_slot_t *slots;
    htab_size_t capacity;
    htab_size_t size;
    htab_size_t used;
    htab_hash_fn hash;
    htab_eq_fn eq;
    htab_free_fn key_free;
    htab_free_fn value_free;
};

// returns the closes min_capacty to the power of 2 to the argument n
static htab_size_t pow2_round_up(htab_size_t n) {
    htab_size_t cap = HTAB_MIN_CAPACITY;
    while (cap < n) {
        if (cap > ULONG_MAX / 2) {
            // next bit shift would overflow
            return 0;
        }
        cap <<= 1;
    }
    return cap;
}

htab *htab_init(htab_size_t initial_capacity, htab_hash_fn hash, htab_eq_fn eq, htab_free_fn key_free, htab_free_fn value_free) {
    if (hash == NULL || eq == NULL) {
        // these functions cannot be unassigned
        return NULL;
    }

    htab *ht = malloc(sizeof(*ht));
    if (ht == NULL) {
        return NULL;
    }

    htab_size_t capacity = pow2_round_up(initial_capacity);
    if (capacity == 0) {
        return NULL;
    }
    if (capacity > SIZE_MAX / sizeof(htab_slot_t)) {
        // malloc would overflow
        return NULL;
    }

    ht->slots = malloc(capacity * sizeof(*ht->slots));
    if (ht->slots == NULL) {
        free(ht);
        return NULL;
    }

    for (htab_size_t i = 0; i < capacity; i++) {
        ht->slots[i].state = SLOT_EMPTY;
    }

    ht->capacity = capacity;
    ht->size = 0;
    ht->used = 0;
    ht->hash = hash;
    ht->eq = eq;
    ht->key_free = key_free;
    ht->value_free = value_free;

    return ht;
}