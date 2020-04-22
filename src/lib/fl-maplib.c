/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * fl-maplib.c: Hashtable (ObjMap) implementation, using open addressing and linear probing
 * See Falcon's license in the LICENSE file
 */

#include "fl-maplib.h"
#include "../vm/fl-mem.h"
#include <stdio.h>
#include <string.h>

/**
 * Frees a previously allocated ObjMap.
 */
void freeMap(FalconVM *vm, ObjMap *map) {
    FALCON_FREE_ARRAY(vm, MapEntry, map->entries, map->capacity);
    map->capacity = 0;
    map->count = 0;
    map->entries = NULL;
}

/**
 * Tries to find an entry for a given key in a given list of MapEntries, that has a given capacity.
 * If found, the entry is returned. Otherwise, returns NULL.
 */
static MapEntry *findEntry(MapEntry *entries, int capacity, ObjString *key) {
    uint32_t index = key->hash % capacity;
    MapEntry *tombstone = NULL;

    while (true) {
        MapEntry *entry = &entries[index];

        if (entry->key == NULL) { /* Checks if the entry is empty */
            if (IS_NULL(entry->value)) {
                return tombstone != NULL ? tombstone : entry;
            } else if (tombstone == NULL) {
                tombstone = entry; /* Tombstone found */
            }
        } else if (entry->key == key) { /* Checks if the key was found */
            return entry;
        }

        index = (index + 1) % capacity; /* Tries the next index */
    }
}

/**
 * Tries to find a ObjString stored in a given ObjMap. To do so, when checking if a key was found,
 * the given length and hash of the string are compared. If a collision happens, the string content
 * is compared as well.
 * If a ObjString is not found in the ObjMap entries, NULL is returned. Otherwise, the found
 * ObjString is returned.
 */
ObjString *mapFindString(ObjMap *map, const char *chars, size_t length, uint32_t hash) {
    if (map->count == 0) return NULL;
    uint32_t index = hash % map->capacity;

    while (true) {
        MapEntry *entry = &map->entries[index];

        if (entry->key == NULL) {
            if (IS_NULL(entry->value)) return NULL;
        } else if (entry->key->length == length && entry->key->hash == hash &&
                   memcmp(entry->key->chars, chars, (size_t) length) ==
                       0) { /* Checks if length, hash, and content match */
            return entry->key;
        }

        index = (index + 1) % map->capacity;
    }
}

/**
 * Tries to get the value corresponding to a given key in a given ObjMap. If an entry for the key
 * cannot be found, "false" is returned. Otherwise, true is returned a "value" is set to point for
 * found value.
 */
bool mapGet(ObjMap *map, ObjString *key, FalconValue *value) {
    if (map->count == 0) return false;
    MapEntry *entry = findEntry(map->entries, map->capacity, key);
    if (entry->key == NULL) return false;
    *value = entry->value;
    return true;
}

/**
 * Adjusts the capacity of a given ObjMap to a new given capacity. To do so, a new dynamic array of
 * entries is allocated with the given capacity, and all the entries are copied.
 */
static void adjustCapacity(FalconVM *vm, ObjMap *map, int capacity) {
    MapEntry *entries = FALCON_ALLOCATE(vm, MapEntry, capacity); /* Allocates the new entries */
    for (int i = 0; i < capacity; i++) {
        entries[i].key = NULL;
        entries[i].value = NULL_VAL;
    }

    map->count = 0;
    for (int i = 0; i < map->capacity; i++) { /* Reallocates the previous entries */
        MapEntry *entry = &map->entries[i];
        if (entry->key == NULL) continue;
        MapEntry *newEntry = findEntry(entries, capacity, entry->key); /* Updates previous entry */
        newEntry->key = entry->key;
        newEntry->value = entry->value;
        map->count++;
    }

    FALCON_FREE_ARRAY(vm, MapEntry, map->entries, map->capacity); /* Frees the old entries */
    map->entries = entries;
    map->capacity = capacity;
}

/**
 * Adds the given key-value pair into the given ObjMap. If an entry for that key is already
 * present, the new value overwrites the old one. Returns a boolean indicating if the key was
 * already existent.
 */
bool mapSet(FalconVM *vm, ObjMap *map, ObjString *key, FalconValue value) {
    if (map->count + 1 > map->capacity * FALCON_TABLE_MAX_LOAD) { /* Max load was reached? */
        int capacity = FALCON_INCREASE_CAPACITY(map->capacity);
        adjustCapacity(vm, map, capacity); /* Adjusts the ObjMap capacity */
    }

    MapEntry *entry = findEntry(map->entries, map->capacity, key);
    bool isNewKey = entry->key == NULL;
    if (isNewKey && IS_NULL(entry->value)) map->count++;
    entry->key = key;
    entry->value = value;
    return isNewKey;
}

/**
 * Tries to delete a key-value pair from a given ObjMap based on a given key. If an entry for the
 * key cannot be found, returns "false". Otherwise, returns "true" and the existing entry is marked
 * as a tombstone.
 */
bool deleteFromMap(ObjMap *map, ObjString *key) {
    if (map->count == 0) return false;
    MapEntry *entry = findEntry(map->entries, map->capacity, key);
    if (entry->key == NULL) return false;

    /* Marks the entry as a tombstone */
    entry->key = NULL;
    entry->value = BOOL_VAL(true);
    return true;
}

/**
 * Copies all the entries from a given ObjMap to another given one.
 */
void copyEntries(FalconVM *vm, ObjMap *from, ObjMap *to) {
    for (int i = 0; i < from->capacity; i++) {
        MapEntry *entry = &from->entries[i];
        if (entry->key != NULL) mapSet(vm, to, entry->key, entry->value);
    }
}

/**
 * Converts a given ObjMap to a ObjString.
 */
ObjString *mapToString(FalconVM *vm, ObjMap *map) {
    size_t allocationSize = MIN_COLLECTION_TO_STR;
    char *string = FALCON_ALLOCATE(vm, char, allocationSize); /* Initial allocation */
    int stringLen = sprintf(string, "{");
    int pairsCount = map->count;

    uint16_t currCount = 0;
    MapEntry *currEntry = map->entries;

    /* Adds the pairs to the string */
    while (true) {
        char *keyString, *valueString;
        size_t keyLen, valueLen, currLen;
        bool isString;

        if (currCount == pairsCount) /* All elements found? */
            break;

        if (currEntry->key != NULL) {
            keyString = currEntry->key->chars;
            keyLen = currEntry->key->length;
            currLen = stringLen + keyLen + 5; /* +5 = quotes (2) + ": " (2) + initial space (1) */

            /* Increases the allocation, if needed */
            if (currLen > allocationSize) {
                int oldSize = allocationSize;
                allocationSize = increaseStringAllocation(currLen, allocationSize);
                string = FALCON_INCREASE_ARRAY(vm, string, char, oldSize, allocationSize);
            }

            /* Appends the current element to the output string */
            stringLen += sprintf(string + stringLen, " \"%s\": ", keyString);

            /* Gets the current value string */
            if (IS_STRING(currEntry->value)) {
                ObjString *objString = AS_STRING(currEntry->value);
                valueString = objString->chars;
                valueLen = objString->length;
                currLen = stringLen + valueLen + 4; /* +4 = quotes (2) + ", " (2) */
                isString = true;
            } else {
                valueString = valueToString(vm, &currEntry->value)->chars;
                valueLen = strlen(valueString);
                currLen = stringLen + valueLen + 2; /* +2 for the ", " */
                isString = false;
            }

            /* Increases the allocation, if needed */
            if (currLen > allocationSize) {
                int oldSize = allocationSize;
                allocationSize = increaseStringAllocation(currLen, allocationSize);
                string = FALCON_INCREASE_ARRAY(vm, string, char, oldSize, allocationSize);
            }

            /* Appends the current value to the output string */
            if (isString) {
                stringLen += sprintf(string + stringLen, "\"%s\"", valueString);
            } else {
                stringLen += sprintf(string + stringLen, "%s", valueString);
            }

            /* Appends the separator or final space */
            if (currCount != pairsCount - 1) {
                stringLen += sprintf(string + stringLen, ",");
            } else {
                stringLen += sprintf(string + stringLen, " ");
            }

            currCount++;
        }

        currEntry++; /* Goes to the next slot */
    }

    sprintf(string + stringLen, "}");
    return falconString(vm, string, strlen(string));
}
