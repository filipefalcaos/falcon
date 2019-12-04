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
    FalconObjString *key;
    FalconValue value;
} Entry;

/* Hashtable representation */
typedef struct {
    int count;
    int capacity;
    Entry *entries; /* Array of hashtable entries */
} FalconTable;

/* Hashtable operations */
void FalconMarkTable(FalconTable *table);
void FalconInitTable(FalconTable *table);
void FalconFreeTable(FalconVM *vm, FalconTable *table);
bool FalconTableGet(FalconTable *table, FalconObjString *key, FalconValue *value);
bool FalconTableSet(FalconVM *vm, FalconTable *table, FalconObjString *key, FalconValue value);
bool FalconTableDelete(FalconTable *table, FalconObjString *key);
FalconObjString *FalconTableFindStr(FalconTable *table, const char *chars, int length,
                                    uint32_t hash);

#endif // FALCON_TABLE_H
