/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * falcon_table.c: Hashtable implementation, using open addressing and linear probing
 * See Falcon's license in the LICENSE file
 */

#ifndef FALCON_TABLE_H
#define FALCON_TABLE_H

#include "../vm/falcon_value.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

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
} Table;

/* Hashtable operations */
void initTable(Table *table);
void freeTable(FalconVM *vm, Table *table);
bool tableGet(Table *table, ObjString *key, FalconValue *value);
bool tableSet(FalconVM *vm, Table *table, ObjString *key, FalconValue value);
bool tableDelete(Table *table, ObjString *key);
ObjString *tableFindStr(Table *table, const char *chars, size_t length, uint32_t hash);
void copyEntries(FalconVM *vm, Table *from, Table *to);

#endif // FALCON_TABLE_H
