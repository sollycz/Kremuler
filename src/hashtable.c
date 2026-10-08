#include "hashtable.h"

#include <assert.h>
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

// calls tables value and key freeing functions if they are specified for a given slot
static void slot_free(const htab *ht, htab_slot_t *slot) {
    if (ht->key_free != NULL) {
        // recast needed because of const type
        ht->key_free((void *)slot->key);
    }
    if (ht->value_free != NULL) {
        ht->value_free(slot->value);
    }
}

void htab_destroy(htab *ht) {
    if (ht == NULL) {
        return;
    }

    for (htab_size_t i = 0; i < ht->capacity; i++) {
        if (ht->slots[i].state == SLOT_USED) {
            slot_free(ht, &ht->slots[i]);
        }
    }

    free(ht->slots);
    free(ht);
}

// linearly probes table, if key is found, returns true and out slot points to it
// if key is not found, returns false and out slot points to the first available slot for insert
static bool probe_slots(const htab *ht, htab_key_t key, htab_size_t hash, htab_slot_t **out_slot) {
    htab_slot_t *deleted_slot = NULL;
    htab_size_t i = hash % ht->capacity;

    for (htab_size_t k = 0; k < ht->capacity; k++) {
        htab_slot_t *s = &ht->slots[i];

        if (s->state == SLOT_EMPTY) {
            if (deleted_slot != NULL) {
                *out_slot = deleted_slot;
            } else {
                *out_slot = s;
            }
            return false;
        }
        if (s->state == SLOT_DELETED) {
            if (deleted_slot == NULL) {
                deleted_slot = s;
            }
        } else if (s->hash == hash && ht->eq(s->key, key)) {
            *out_slot = s;
            return true;
        }

        i++;
        if (i == ht->capacity) {
            i = 0;
        }
    }

    // this can return NULL, which needs to be taken into account for insert
    *out_slot = deleted_slot;
    return false;
}

bool htab_get(const htab *ht, htab_key_t key, htab_value_t *out) {
    if (ht == NULL) {
        return false;
    }

    htab_size_t hash = ht->hash(key);

    htab_slot_t *ret_slot;
    bool ret_bool = probe_slots(ht, key, hash, &ret_slot);

    if (ret_bool == false) {
        return false;
    }
    if (out != NULL) {
        *out = ret_slot->value;
    }
    return true;
}

// TODO: remake htab_res_t and get rid of assert
htab_res_t htab_insert(htab *ht, htab_key_t key, htab_value_t value) {
    // TODO: replace
    assert(ht != NULL);

    htab_size_t hash = ht->hash(key);

    // TODO: check load factor / expand table

    htab_slot_t *slot;
    if (probe_slots(ht, key, hash, &slot)) {
        return HTAB_EXISTS;
    }
    // TODO: replace
    assert(slot != NULL);

    if (slot->state == SLOT_EMPTY) {
        // temp fix before resizing for testing
        // TODO: replace
        assert(ht->used + 1 < ht->capacity);
        ht->used++;
    }
    ht->size++;

    slot->state = SLOT_USED;
    slot->hash = hash;
    slot->key = key;
    slot->value = value;

    return HTAB_OK;
}

bool htab_remove(htab *ht, htab_key_t key) {
    // TODO: replace
    assert(ht != NULL);

    htab_size_t hash = ht->hash(key);

    htab_slot_t *slot;
    if (!probe_slots(ht, key, hash, &slot)) {
        return false;
    }

    slot_free(ht, slot);
    ht->size--;

    slot->state = SLOT_DELETED;

    // temp for testing, can be removed
    // TODO
    slot->key = NULL;
    slot->value = NULL;

    return true;
}

htab_size_t htab_size(const htab *ht) {
    // TODO: decide on error handling / replace asserts
    assert(ht != NULL);
    return ht->size;
}

void htab_foreach(const htab *ht, htab_foreach_fn fn) {
    assert(ht != NULL);
    assert(fn != NULL);

    htab_slot_t *slot;
    for (htab_size_t i = 0; i < ht->capacity; i++) {
        slot = &ht->slots[i];
        if (slot->state == SLOT_USED) {
            fn(slot->key, slot->value);
        }
    }
}