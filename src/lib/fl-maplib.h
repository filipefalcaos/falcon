/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * fl-maplib.h: Hashtable (ObjMap) implementation, using open addressing and linear probing
 * See Falcon's license in the LICENSE file
 */

#ifndef FL_MAPLIB_H
#define FL_MAPLIB_H

#include "../core/fl-object.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* The hashtable max load factor */
#define FALCON_TABLE_MAX_LOAD 0.75

/* Frees a previously allocated ObjMap */
void free_map(FalconVM *vm, ObjMap *map);

/* Tries to get the value corresponding to a given key in a given ObjMap. If an entry for the key
 * cannot be found, "false" is returned. Otherwise, true is returned a "value" is set to point for
 * found value */
bool map_get(ObjMap *map, ObjString *key, FalconValue *value);

/* Adds the given key-value pair into the given ObjMap. Returns a boolean indicating if the key was
 * already existent */
bool map_set(FalconVM *vm, ObjMap *map, ObjString *key, FalconValue value);

/* Tries to delete a key-value pair from a given ObjMap based on a given key. Returns a boolean
 * indicating if the deletion was successful */
bool map_remove(ObjMap *map, ObjString *key);

/* Tries to find a ObjString stored in a given ObjMap. If a ObjString is not found in the ObjMap
 * entries, NULL is returned. Otherwise, the found ObjString is returned */
ObjString *find_string(ObjMap *map, const char *chars, size_t length, uint32_t hash);

/* Copies all the entries from a given ObjMap to another given one */
void copy_entries(FalconVM *vm, ObjMap *from, ObjMap *to);

/* Converts a given ObjMap to a ObjString */
ObjString *map_to_string(FalconVM *vm, ObjMap *map);

#endif // FL_MAPLIB_H
