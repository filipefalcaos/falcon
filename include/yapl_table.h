/*
 * YAPL version 0.0.1 (master, Oct 18 2019)
 * yapl_table.h: Hashtable implementation, using open addressing and linear probing
 * See YAPL's license in the LICENSE file
 */

#ifndef YAPL_TABLE_H
#define YAPL_TABLE_H

#include "commons.h"
#include "yapl_value.h"

/* Hashtable entry representation */
typedef struct {
    ObjString *key;
    Value value;
} Entry;

/* Hashtable representation */
typedef struct {
    int count;
    int capacity;
    Entry *entries; /* Array of hashtable entries */
} Table;

/* Hashtable operations */
void initTable(Table *table);
void freeTable(Table *table);
bool tableGet(Table *table, ObjString *key, Value *value);
bool tableSet(Table *table, ObjString *key, Value value);
bool tableDelete(Table *table, ObjString *key);
ObjString *tableFindStr(Table *table, const char *chars, int length, uint32_t hash);

#endif // YAPL_TABLE_H
