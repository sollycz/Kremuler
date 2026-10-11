#include "hashtable.h"

#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>

// out of a 100
#define HTAB_LOAD_FACTOR 75
#define HTAB_DELETED_LOAD_FACTOR 25

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

htab *htab_init(htab_size_t initial_capacity, htab_hash_fn hash, htab_eq_fn eq, htab_free_fn key_free, htab_free_fn value_free) {
    assert(hash != NULL);
    assert(eq != NULL);

    if (initial_capacity < 8) {
        initial_capacity = 8;
    } else if (initial_capacity > SIZE_MAX / sizeof(htab_slot_t)) {
        return NULL;
    }

    htab_size_t capacity = initial_capacity;

    htab *ht = malloc(sizeof(*ht));
    if (ht == NULL) {
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

// frees given slot if htab has free fns set
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

// linearly probes table, returns true if slot is found and out_slot points to it.
// if key is not found, returns false and out slot points to the first available slot for insert.
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
    assert(ht != NULL);

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

// resizes slots for new_capacity and clears all deleted slots.
// return true if succesfull, otherwise false. (table is left unchanged)
static bool resize(htab *ht, htab_size_t new_capacity) {
    assert(ht->capacity <= new_capacity);

    if (new_capacity > SIZE_MAX / sizeof(htab_slot_t)) {
        // malloc would overflow
        return false;
    }

    htab_slot_t *new_slots = malloc(new_capacity * sizeof(*new_slots));
    if (new_slots == NULL) {
        return false;
    }

    for (htab_size_t i = 0; i < new_capacity; i++) {
        new_slots[i].state = SLOT_EMPTY;
    }

    htab_slot_t *old_slot;
    htab_size_t new_index;
    for (htab_size_t i = 0; i < ht->capacity; i++) {
        old_slot = &ht->slots[i];
        if (old_slot->state == SLOT_USED) {
            new_index = old_slot->hash % new_capacity;
            while (new_slots[new_index].state != SLOT_EMPTY) {
                new_index++;
                if (new_index == new_capacity) {
                    new_index = 0;
                }
            }
            new_slots[new_index] = *old_slot;
        }
    }

    free(ht->slots);
    ht->slots = new_slots;
    ht->capacity = new_capacity;
    ht->used = ht->size;
    return true;
}

htab_res_t htab_insert(htab *ht, htab_key_t key, htab_value_t value) {
    assert(ht != NULL);

    htab_size_t hash = ht->hash(key);

    htab_slot_t *slot;
    if (probe_slots(ht, key, hash, &slot)) {
        return HTAB_EXISTS;
    }
    assert(slot != NULL);

    if (slot->state == SLOT_EMPTY) {
        htab_size_t cur_max_size = ht->capacity * HTAB_LOAD_FACTOR / 100;

        if (ht->used >= cur_max_size) {
            htab_size_t new_capacity = ht->capacity;

            if (ht->used - ht->size < ht->capacity * HTAB_DELETED_LOAD_FACTOR / 100) {
                new_capacity *= 2;

                if (new_capacity > ULONG_MAX) {
                    return HTAB_NOMEM;
                }
            }

            if (!resize(ht, new_capacity)) {
                return HTAB_NOMEM;
            }
            bool found = probe_slots(ht, key, hash, &slot);
            assert(!found && slot != NULL);
        }
    }

    if (slot->state == SLOT_EMPTY) {
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
    assert(ht != NULL);

    htab_size_t hash = ht->hash(key);

    htab_slot_t *slot;
    if (!probe_slots(ht, key, hash, &slot)) {
        return false;
    }

    slot_free(ht, slot);
    ht->size--;

    slot->state = SLOT_DELETED;

    // not nescessery
    slot->key = NULL;
    slot->value = NULL;

    return true;
}

htab_size_t htab_size(const htab *ht) {
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
