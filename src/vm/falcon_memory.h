/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * falcon_memory.h: Falcon's automatic memory manager
 * See Falcon's license in the LICENSE file
 */

#ifndef FALCON_MEMORY_H
#define FALCON_MEMORY_H

#include "falcon_object.h"
#include "falcon_vm.h"

/* Allocates an array with a given element type and count */
#define FALCON_ALLOCATE(vm, type, count) \
    (type *) falconReallocate(vm, NULL, 0, sizeof(type) * (count))

/* Allocates an object of a given type */
#define FALCON_ALLOCATE_OBJ(vm, type, objectType) \
    (type *) falconAllocateObj(vm, sizeof(type), objectType)

/* Frees an allocation by resizing it to zero bytes */
#define FALCON_FREE(vm, type, pointer) falconReallocate(vm, pointer, sizeof(type), 0)

/* Doubles the capacity of a given dynamic array based on its current one */
#define FALCON_INCREASE_CAPACITY(capacity) ((capacity) < 8 ? 8 : (capacity) * (2))

/* Increases the allocation of a given dynamic array */
#define FALCON_INCREASE_ARRAY(vm, previous, type, oldCount, count) \
    (type *) falconReallocate(vm, previous, sizeof(type) * (oldCount), sizeof(type) * (count))

/* Frees the memory allocated on a dynamic array by passing in zero for the new size to the
 * "falconReallocate" function */
#define FALCON_FREE_ARRAY(vm, type, pointer, oldCount) \
    falconReallocate(vm, pointer, sizeof(type) * (oldCount), 0)

/* Out of memory error */
#define FALCON_OUT_OF_MEM_ERR "MemoryError: Failed to allocate memory."

/* Memory management operations */
void falconMemoryError();
void *falconReallocate(FalconVM *vm, void *previous, size_t oldSize, size_t newSize);
FalconObj *falconAllocateObj(FalconVM *vm, size_t size, ObjType type);
void falconFreeObj(FalconVM *vm, FalconObj *object);
void falconFreeObjs(FalconVM *vm);

#endif // FALCON_MEMORY_H
