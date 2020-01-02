/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * falcon_memory.c: Falcon's automatic memory manager
 * See Falcon's license in the LICENSE file
 */

#include "falcon_memory.h"
#include "falcon_gc.h"
#include <stdio.h>
#include <stdlib.h>

#ifdef FALCON_DEBUG_LEVEL_02
#include "falcon_debug.h"
#endif

/**
 * Presents a message indicating that a memory allocation error occurred.
 */
void falconMemoryError() {
    fprintf(stderr, FALCON_OUT_OF_MEM_ERR);
    fprintf(stderr, "\n");
    exit(FALCON_ERR_MEMORY);
}

/**
 * Handles all dynamic memory management — allocating memory, freeing it, and changing the size of
 * an existing allocation.
 */
void *falconReallocate(FalconVM *vm, void *previous, size_t oldSize, size_t newSize) {
    vm->bytesAllocated += newSize - oldSize;

    if (newSize > oldSize) { /* More memory allocation? */
#ifdef FALCON_DEBUG_STRESS_GC
        falconRunGC(vm); /* Runs the garbage collector always */
#else
        if (vm->bytesAllocated > vm->nextGC)
            falconRunGC(vm); /* Runs the garbage collector, if needed */
#endif
    }

    if (newSize == 0) { /* Deallocate memory? */
        free(previous);
        return NULL;
    }

    void *allocatedMemory = realloc(previous, newSize); /* Reallocates memory */
    if (allocatedMemory == NULL) {                      /* Allocation failed? */
        falconRunGC(vm); /* Runs the garbage collector to free some memory */
        allocatedMemory = realloc(previous, newSize); /* Retries to reallocate memory */

        if (allocatedMemory == NULL) /* Allocation still failed? */
            falconMemoryError();     /* No memory available, exit */
    }

    return allocatedMemory;
}

/**
 * Allocates a new Falcon object.
 */
FalconObj *falconAllocateObj(FalconVM *vm, size_t size, ObjType type) {
    FalconObj *object = (FalconObj *) falconReallocate(vm, NULL, 0, size); /* Sets a new object */
    object->isMarked = false;
    object->type = type;        /* Sets the object type */
    object->next = vm->objects; /* Adds the new object to the object list */
    vm->objects = object;

#ifdef FALCON_DEBUG_LEVEL_02
    dumpAllocation(object, size, type);
#endif

    return object;
}

/**
 * Frees a given allocated object.
 */
void falconFreeObj(FalconVM *vm, FalconObj *object) {
#ifdef FALCON_DEBUG_LEVEL_02
    dumpFree(object);
#endif

    switch (object->type) {
        case OBJ_STRING: {
            ObjString *string = (ObjString *) object;
            falconReallocate(vm, object, sizeof(ObjString) + string->length + 1, 0);
            break;
        }
        case OBJ_FUNCTION: {
            ObjFunction *function = (ObjFunction *) object;
            freeBytecode(vm, &function->bytecode);
            FALCON_FREE(vm, ObjFunction, object);
            break;
        }
        case OBJ_UPVALUE:
            FALCON_FREE(vm, ObjUpvalue, object);
            break;
        case OBJ_CLOSURE: {
            ObjClosure *closure = (ObjClosure *) object;
            FALCON_FREE_ARRAY(vm, ObjUpvalue *, closure->upvalues, closure->upvalueCount);
            FALCON_FREE(vm, ObjClosure, object);
            break;
        }
        case OBJ_CLASS:
            FALCON_FREE(vm, ObjClass, object);
            break;
        case OBJ_INSTANCE: {
            ObjInstance *instance = (ObjInstance *) object;
            freeTable(vm, &instance->fields);
            FALCON_FREE(vm, ObjInstance, object);
            break;
        }
        case OBJ_LIST: {
            ObjList *list = (ObjList *) object;
            FALCON_FREE_ARRAY(vm, FalconValue, list->elements.values, list->elements.capacity);
            FALCON_FREE(vm, ObjList, object);
            break;
        }
        case OBJ_NATIVE:
            FALCON_FREE(vm, ObjNative, object);
            break;
    }
}

/**
 * Frees all the objects allocated in the virtual machine.
 */
void falconFreeObjs(FalconVM *vm) {
    FalconObj *object = vm->objects;
    while (object != NULL) {
        FalconObj *next = object->next;
        falconFreeObj(vm, object);
        object = next;
    }

    free(vm->grayStack); /* Frees the GC's grey stack */
}
