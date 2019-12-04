/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * falcon_table.c: Hashtable implementation, using open addressing and linear probing
 * See Falcon's license in the LICENSE file
 */

#include "falcon_table.h"
#include "../vm/falcon_memory.h"
#include <string.h>

/* The hashtable max load factor */
#define FALCON_TABLE_MAX_LOAD 0.75

/**
 * Marks every key/value pair in the hashtable for garbage collection.
 */
void FalconMarkTable(FalconTable *table) {
    for (int i = 0; i < table->capacity; i++) {
        Entry *entry = &table->entries[i];
        FalconMarkObject((FalconObj *) entry->key);
        FalconMarkValue(entry->value);
    }
}

/**
 * Initializes an empty hashtable.
 */
void FalconInitTable(FalconTable *table) {
    table->count = 0;
    table->capacity = 0;
    table->entries = NULL;
}

/**
 * Frees a hashtable.
 */
void FalconFreeTable(FalconVM *vm, FalconTable *table) {
    FALCON_FREE_ARRAY(vm, Entry, table->entries, table->capacity);
    FalconInitTable(table);
}

/**
 * Find an entry for a given key.
 */
static Entry *findEntry(Entry *entries, int capacity, FalconObjString *key) {
    uint32_t index = key->hash % capacity;
    Entry *tombstone = NULL;

    while (true) {
        Entry *entry = &entries[index];

        if (entry->key == NULL) { /* Checks if the entry is empty */
            if (FALCON_IS_NULL(entry->value))
                return tombstone != NULL ? tombstone : entry;
            else if (tombstone == NULL)
                tombstone = entry;      /* Tombstone found */
        } else if (entry->key == key) { /* Checks if the key was found */
            return entry;
        }

        index = (index + 1) % capacity; /* Tries the next index */
    }
}

/**
 * Gets the value corresponding to a given key in a hashtable.
 */
bool FalconTableGet(FalconTable *table, FalconObjString *key, FalconValue *value) {
    if (table->count == 0) return false;
    Entry *entry = findEntry(table->entries, table->capacity, key);
    if (entry->key == NULL) return false;
    *value = entry->value;
    return true;
}

/**
 * Adjusts the capacity of a hashtable by initializing the added empty items and updating entries
 * of the original hashtable.
 */
static void adjustCapacity(FalconVM *vm, FalconTable *table, int capacity) {
    Entry *entries = FALCON_ALLOCATE(vm, Entry, capacity); /* Allocates new HashTable entries */
    if (entries == NULL) {                                 /* Checks if the allocation failed */
        FalconMemoryError();
        return;
    }

    for (int i = 0; i < capacity; i++) { /* Allocates the new entries */
        entries[i].key = NULL;
        entries[i].value = FALCON_NULL_VAL;
    }

    table->count = 0;
    for (int i = 0; i < table->capacity; i++) { /* Reallocates the previous entries */
        Entry *entry = &table->entries[i];
        if (entry->key == NULL) continue;
        Entry *newEntry = findEntry(entries, capacity, entry->key); /* Updates a previous entry */
        newEntry->key = entry->key;
        newEntry->value = entry->value;
        table->count++;
    }

    FALCON_FREE_ARRAY(vm, Entry, table->entries, table->capacity); /* Frees the old hashtable */
    table->entries = entries;
    table->capacity = capacity;
}

/**
 * Adds the given key/value pair into the given hashtable. If an entry for that key is already
 * present, the new value overwrites the old.
 */
bool FalconTableSet(FalconVM *vm, FalconTable *table, FalconObjString *key, FalconValue value) {
    if (table->count + 1 > table->capacity * FALCON_TABLE_MAX_LOAD) { /* Max load was reached? */
        int capacity = FALCON_INCREASE_CAPACITY(table->capacity);
        adjustCapacity(vm, table, capacity); /* Adjust the hashtable capacity */
    }

    Entry *entry = findEntry(table->entries, table->capacity, key);
    bool isNewKey = entry->key == NULL;
    if (isNewKey && FALCON_IS_NULL(entry->value)) table->count++;
    entry->key = key;
    entry->value = value;
    return isNewKey;
}

/**
 * Deletes a key/value pair from a given hashtable.
 */
bool FalconTableDelete(FalconTable *table, FalconObjString *key) {
    if (table->count == 0) return false;
    Entry *entry = findEntry(table->entries, table->capacity, key);
    if (entry->key == NULL) return false;
    entry->key = NULL;
    entry->value = FALCON_BOOL_VAL(true);
    return true;
}

/**
 * Finds whether a string is set in a hashtable or not.
 */
FalconObjString *FalconTableFindStr(FalconTable *table, const char *chars, int length,
                                    uint32_t hash) {
    if (table->count == 0) return NULL;
    uint32_t index = hash % table->capacity;

    while (true) {
        Entry *entry = &table->entries[index];

        if (entry->key == NULL) {
            if (FALCON_IS_NULL(entry->value)) return NULL;
        } else if (entry->key->length == length && entry->key->hash == hash &&
                   memcmp(entry->key->chars, chars, (size_t) length) ==
                       0) { /* Checks if length, hash, and content match */
            return entry->key;
        }

        index = (index + 1) % table->capacity;
    }
}
