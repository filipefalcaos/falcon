/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * falcon_table.c: Hashtable implementation, using open addressing and linear probing
 * See Falcon's license in the LICENSE file
 */

#ifndef FALCON_TABLE_H
#define FALCON_TABLE_H

#include "../commons.h"
#include "../vm/falcon_value.h"

/* Hashtable entry representation */
typedef struct {
    ObjString *key;
    FalconValue value;
} Entry;

/* Hashtable representation */
typedef struct {
    int count;
    int capacity;
    Entry *entries; /* Array of hashtable entries */
} FalconTable;

/* Hashtable operations */
void falconInitTable(FalconTable *table);
void falconFreeTable(FalconVM *vm, FalconTable *table);
bool falconTableGet(FalconTable *table, ObjString *key, FalconValue *value);
bool falconTableSet(FalconVM *vm, FalconTable *table, ObjString *key, FalconValue value);
bool falconTableDelete(FalconTable *table, ObjString *key);
ObjString *falconTableFindStr(FalconTable *table, const char *chars, size_t length, uint32_t hash);

#endif // FALCON_TABLE_H
