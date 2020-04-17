/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * falcon_map.h: Falcon's standard map library
 * See Falcon's license in the LICENSE file
 */

#ifndef FALCON_MAP_H
#define FALCON_MAP_H

#include "../vm/falcon_object.h"

/* Map functions */
ObjString *mapToString(FalconVM *vm, ObjMap *map);

#endif // FALCON_MAP_H
