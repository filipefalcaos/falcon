/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * fl-mem.h: Falcon's automatic memory manager
 * See Falcon's license in the LICENSE file
 */

#ifndef FL_MEM_H
#define FL_MEM_H

#include "fl-gc.h"
#include "fl-object.h"
#include "fl-vm.h"

/* Allocates a dynamic array of a given element type and count */
#define FALCON_ALLOCATE(vm, type, count) (type *) reallocate(vm, NULL, 0, sizeof(type) * (count))

/* Allocates a FalconObj of a given type */
#define FALCON_ALLOCATE_OBJ(vm, type, objectType) \
    (type *) allocate_object(vm, sizeof(type), objectType)

/* Frees an allocation by calling "reallocate" with "newSize" set to 0 */
#define FALCON_FREE(vm, type, pointer) reallocate(vm, pointer, sizeof(type), 0)

/* Increases a given capacity based on its current one. This macro should be used to increase the
 * capacity of a dynamic array right before calling the "FALCON_ALLOCATE" macro */
#define FALCON_INCREASE_CAPACITY(capacity) ((capacity) < 8 ? 8 : (capacity) * (2))

/* Increases the allocation of a given dynamic array from "oldCount" to "count" */
#define FALCON_INCREASE_ARRAY(vm, previous, type, oldCount, count) \
    (type *) reallocate(vm, previous, sizeof(type) * (oldCount), sizeof(type) * (count))

/* Frees the memory allocated to a dynamic array by calling "reallocate" with "newSize" set
 * to 0 */
#define FALCON_FREE_ARRAY(vm, type, pointer, oldCount) \
    reallocate(vm, pointer, sizeof(type) * (oldCount), 0)

/* Out of memory error message */
#define FALCON_OUT_OF_MEM_ERR "MemoryError: Failed to allocate memory."

/* Prints a message to stderr indicating that a memory allocation error occurred */
void memory_error();

/* Handles all dynamic memory management: allocating memory, freeing it, and changing the size of
 * an existing allocation */
void *reallocate(FalconVM *vm, void *previous, size_t oldSize, size_t newSize);

/* Allocates a new FalconObj with a given size and initializes the object fields */
FalconObj *allocate_object(FalconVM *vm, size_t size, ObjType type);

/* Frees the memory previously allocated to a given FalconObj */
void free_object(FalconVM *vm, FalconObj *object);

/* Frees the memory previously allocated to all FalconObjs allocated by the virtual machine */
void free_vm_objects(FalconVM *vm);

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

#endif // FL_MEM_H
