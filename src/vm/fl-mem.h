/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * fl-mem.h: Falcon's automatic memory manager
 * See Falcon's license in the LICENSE file
 */

#ifndef FALCON_MEMORY_H
#define FALCON_MEMORY_H

#include "fl-gc.h"
#include "fl-object.h"
#include "fl-vm.h"

/* Allocates a dynamic array of a given element type and count */
#define FALCON_ALLOCATE(vm, type, count) \
    (type *) falconReallocate(vm, NULL, 0, sizeof(type) * (count))

/* Allocates a FalconObj of a given type */
#define FALCON_ALLOCATE_OBJ(vm, type, objectType) \
    (type *) falconAllocateObj(vm, sizeof(type), objectType)

/* Frees an allocation by calling "falconReallocate" with "newSize" set to 0 */
#define FALCON_FREE(vm, type, pointer) falconReallocate(vm, pointer, sizeof(type), 0)

/* Increases a given capacity based on its current one. This macro should be used to increase the
 * capacity of a dynamic array right before calling the "FALCON_ALLOCATE" macro */
#define FALCON_INCREASE_CAPACITY(capacity) ((capacity) < 8 ? 8 : (capacity) * (2))

/* Increases the allocation of a given dynamic array from "oldCount" to "count" */
#define FALCON_INCREASE_ARRAY(vm, previous, type, oldCount, count) \
    (type *) falconReallocate(vm, previous, sizeof(type) * (oldCount), sizeof(type) * (count))

/* Frees the memory allocated to a dynamic array by calling "falconReallocate" with "newSize" set
 * to 0 */
#define FALCON_FREE_ARRAY(vm, type, pointer, oldCount) \
    falconReallocate(vm, pointer, sizeof(type) * (oldCount), 0)

/* Out of memory error message */
#define FALCON_OUT_OF_MEM_ERR "MemoryError: Failed to allocate memory."

/* Prints a message to stderr indicating that a memory allocation error occurred */
void falconMemoryError();

/* Handles all dynamic memory management: allocating memory, freeing it, and changing the size of
 * an existing allocation */
void *falconReallocate(FalconVM *vm, void *previous, size_t oldSize, size_t newSize);

/* Allocates a new FalconObj with a given size and initializes the object fields */
FalconObj *falconAllocateObj(FalconVM *vm, size_t size, ObjType type);

/* Frees the memory previously allocated to a given FalconObj */
void falconFreeObj(FalconVM *vm, FalconObj *object);

/* Frees the memory previously allocated to all FalconObjs allocated by the virtual machine */
void falconFreeObjs(FalconVM *vm);

/**
 * Increases the allocation size of a string based on its current length.
 */
static inline size_t increaseStringAllocation(size_t currLen, size_t allocationSize) {
    if (currLen > (allocationSize * 2)) {
        return currLen + allocationSize * 2;
    } else {
        return FALCON_INCREASE_CAPACITY(allocationSize);
    }
}

#endif // FALCON_MEMORY_H
