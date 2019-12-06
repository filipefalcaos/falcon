/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * falcon_memory.c: Falcon's automatic memory manager
 * See Falcon's license in the LICENSE file
 */

#include "falcon_memory.h"
#include "falcon_gc.h"
#include <stdio.h>
#include <stdlib.h>

#ifdef FALCON_DEBUG_LOG_MEMORY
#include "falcon_debug.h"
#endif

/**
 * Presents a message indicating that a memory allocation error occurred.
 */
void FalconMemoryError() {
    fprintf(stderr, FALCON_OUT_OF_MEM_ERR);
    fprintf(stderr, "\n");
    exit(1);
}

/**
 * Handles all dynamic memory management — allocating memory, freeing it, and changing the size of
 * an existing allocation.
 */
void *FalconReallocate(FalconVM *vm, void *previous, size_t oldSize, size_t newSize) {
    vm->bytesAllocated += newSize - oldSize;

    if (newSize > oldSize) { /* More memory allocation? */
#ifdef FALCON_DEBUG_STRESS_GC
        FalconRunGC(vm); /* Runs the garbage collector always */
#else
        if (vm->bytesAllocated > vm->nextGC)
            FalconRunGC(vm); /* Runs the garbage collector, if needed */
#endif
    }

    if (newSize == 0) { /* Deallocate memory? */
        free(previous);
        return NULL;
    }

    return realloc(previous, newSize); /* Reallocate memory */
}

/**
 * Allocates a new Falcon object.
 */
FalconObj *FalconAllocateObject(FalconVM *vm, size_t size, FalconObjType type) {
    FalconObj *object = (FalconObj *) FalconReallocate(vm, NULL, 0, size); /* Sets a new object */
    if (object == NULL) { /* Checks if the allocation failed */
        FalconMemoryError();
        return NULL;
    }

    object->isMarked = false;
    object->type = type;        /* Sets the object type */
    object->next = vm->objects; /* Adds the new object to the object list */
    vm->objects = object;

#ifdef FALCON_DEBUG_LOG_MEMORY
    FalconDumpAllocation(object, size, type);
#endif

    return object;
}

/**
 * Frees a given allocated object.
 */
void freeObject(FalconVM *vm, FalconObj *object) {
#ifdef FALCON_DEBUG_LOG_MEMORY
    FalconDumpFree(object);
#endif

    switch (object->type) {
        case FALCON_OBJ_STRING: {
            FalconObjString *string = (FalconObjString *) object;
            FalconReallocate(vm, object, sizeof(FalconObjString) + string->length + 1, 0);
            break;
        }
        case FALCON_OBJ_UPVALUE:
            FALCON_FREE(vm, FalconObjUpvalue, object);
            break;
        case FALCON_OBJ_CLOSURE: {
            FalconObjClosure *closure = (FalconObjClosure *) object;
            FALCON_FREE_ARRAY(vm, FalconObjUpvalue *, closure->upvalues, closure->upvalueCount);
            FALCON_FREE(vm, FalconObjClosure, object);
            break;
        }
        case FALCON_OBJ_FUNCTION: {
            FalconObjFunction *function = (FalconObjFunction *) object;
            FalconFreeBytecode(vm, &function->bytecodeChunk);
            FALCON_FREE(vm, FalconObjFunction, object);
            break;
        }
        case FALCON_OBJ_NATIVE:
            FALCON_FREE(vm, FalconObjNative, object);
            break;
    }
}

/**
 * Frees all the objects allocated in the virtual machine.
 */
void FalconFreeObjects(FalconVM *vm) {
    FalconObj *object = vm->objects;
    while (object != NULL) {
        FalconObj *next = object->next;
        freeObject(vm, object);
        object = next;
    }

    free(vm->grayStack); /* Frees the GC's grey stack */
}
