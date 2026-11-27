/**
 * @file hashtable.c
 * @brief Implementation of Hashtable ADT
 * @author Filip Radulovič (xradulf00)
 */

#include "hashtable.h"

#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>

// out of a 100
#define HTAB_LOAD_FACTOR 75 // grow when used slots reach this
#define HTAB_DELETED_LOAD_FACTOR 25 // grow / remove (rehash) to restore capacity

typedef enum { SLOT_EMPTY,
    SLOT_USED,
    SLOT_DELETED } SlotState;

typedef struct {
    SlotState state;
    HtabSize hash;
    HtabKey key;
    HtabValue value;
} HtabSlot;

struct Htab {
    HtabSlot* slots;
    HtabSize capacity;
    HtabSize size;
    HtabSize used;
    HtabHashFn hash;
    HtabEqFn eq;
    HtabFreeFn key_free;
    HtabFreeFn value_free;
};

Htab* htab_init(
    HtabSize initial_capacity, HtabHashFn hash, HtabEqFn eq, HtabFreeFn key_free, HtabFreeFn value_free)
{
    assert(hash != NULL);
    assert(eq != NULL);

    if (initial_capacity < 8) {
        initial_capacity = 8;
    } else if (initial_capacity > SIZE_MAX / sizeof(HtabSlot)) {
        return NULL;
    }

    HtabSize capacity = initial_capacity;

    Htab* ht = malloc(sizeof(*ht));
    if (ht == NULL) {
        return NULL;
    }

    ht->slots = malloc(capacity * sizeof(*ht->slots));
    if (ht->slots == NULL) {
        free(ht);
        return NULL;
    }

    for (HtabSize i = 0; i < capacity; i++) {
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
static void slot_free(const Htab* ht, HtabSlot* slot)
{
    if (ht->key_free != NULL) {
        // recast needed because of const type
        ht->key_free((void*)slot->key);
    }
    if (ht->value_free != NULL) {
        ht->value_free(slot->value);
    }
}

void htab_destroy(Htab* ht)
{
    if (ht == NULL) {
        return;
    }

    for (HtabSize i = 0; i < ht->capacity; i++) {
        if (ht->slots[i].state == SLOT_USED) {
            slot_free(ht, &ht->slots[i]);
        }
    }

    free(ht->slots);
    free(ht);
}

// linearly probes table, returns true if slot is found and out_slot points to it.
// if key is not found, returns false and out slot points to the first available slot for insert.
static bool probe_slots(const Htab* ht, HtabKey key, HtabSize hash, HtabSlot** out_slot)
{
    HtabSlot* deleted_slot = NULL;
    HtabSize i = hash % ht->capacity;

    // in the worst case scenario go through one full lap
    for (HtabSize k = 0; k < ht->capacity; k++) {
        HtabSlot* s = &ht->slots[i];

        // first empty slot ends the search, the key can not be any further
        if (s->state == SLOT_EMPTY) {
            if (deleted_slot != NULL) {
                *out_slot = deleted_slot;
            } else {
                *out_slot = s;
            }
            return false;
        }

        // keep the first deleted slot for insert
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

bool htab_get(const Htab* ht, HtabKey key, HtabValue* out)
{
    assert(ht != NULL);

    HtabSize hash = ht->hash(key);

    HtabSlot* ret_slot;
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
// return true if successful, otherwise false. (table is left unchanged)
static bool resize(Htab* ht, HtabSize new_capacity)
{
    assert(ht->capacity <= new_capacity);

    if (new_capacity > SIZE_MAX / sizeof(HtabSlot)) {
        // malloc would allocate wrong size thanks to unsigned wraparound
        return false;
    }

    HtabSlot* new_slots = malloc(new_capacity * sizeof(*new_slots));
    if (new_slots == NULL) {
        return false;
    }

    for (HtabSize i = 0; i < new_capacity; i++) {
        new_slots[i].state = SLOT_EMPTY;
    }

    HtabSlot* old_slot;
    HtabSize new_index;
    for (HtabSize i = 0; i < ht->capacity; i++) {
        old_slot = &ht->slots[i];
        if (old_slot->state == SLOT_USED) {
            new_index = old_slot->hash % new_capacity;
            // there are no deleted slots in fresh table, first empty is the correct one
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

HtabRes htab_insert(Htab* ht, HtabKey key, HtabValue value)
{
    assert(ht != NULL);

    HtabSize hash = ht->hash(key);

    HtabSlot* slot;
    if (probe_slots(ht, key, hash, &slot)) {
        return HTAB_EXISTS;
    }
    assert(slot != NULL);

    // using an empty slot raises used -> check if grow is needed
    if (slot->state == SLOT_EMPTY) {
        HtabSize cur_max_size = ht->capacity * HTAB_LOAD_FACTOR / 100;

        if (ht->used >= cur_max_size) {
            HtabSize new_capacity = ht->capacity;

            // if deleted slots are under DELETE_LOAD_FACTOR -> table is too full, double it
            // if deleted slots are above -> rehash at the same capacity, thus clearing deleted slots
            if (ht->used - ht->size < ht->capacity * HTAB_DELETED_LOAD_FACTOR / 100) {
                if (new_capacity > ULONG_MAX / 2) {
                    return HTAB_NOMEM;
                }
                new_capacity *= 2;
            }

            if (!resize(ht, new_capacity)) {
                return HTAB_NOMEM;
            }
            // slots were moved (rehash or resize), find the first insert position and reinsert into the new table
            bool found = probe_slots(ht, key, hash, &slot);
            assert(!found && slot != NULL);
        }
    }

    // reusing a deleted slot does not change used
    // using empty one does
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

bool htab_remove(Htab* ht, HtabKey key)
{
    assert(ht != NULL);

    HtabSize hash = ht->hash(key);

    HtabSlot* slot;
    if (!probe_slots(ht, key, hash, &slot)) {
        return false;
    }

    slot_free(ht, slot);
    ht->size--;

    slot->state = SLOT_DELETED;

    // not needed, but makes for easier debugging later
    slot->key = NULL;
    slot->value = NULL;

    return true;
}

HtabSize htab_size(const Htab* ht)
{
    assert(ht != NULL);
    return ht->size;
}
