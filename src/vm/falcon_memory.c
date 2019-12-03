/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * falcon_memory.c: Falcon's automatic memory manager
 * See Falcon's license in the LICENSE file
 */

#include "falcon_memory.h"
#include "../lib/falcon_natives.h"
#include <stdio.h>
#include <stdlib.h>

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
void *FalconReallocate(void *previous, size_t oldSize, size_t newSize) {
    if (newSize == 0) {
        free(previous);
        return NULL;
    }

    return realloc(previous, newSize);
}

/**
 * Allocates a new Falcon object.
 */
FalconObj *FalconAllocateObject(VM *vm, size_t size, FalconObjType type) {
    FalconObj *object = (FalconObj *) FalconReallocate(NULL, 0, size); /* Creates a new object */
    if (object == NULL) { /* Checks if the allocation failed */
        FalconMemoryError();
        return NULL;
    }

    object->type = type;        /* Sets the object type */
    object->next = vm->objects; /* Adds the new object to the object list */
    vm->objects = object;
    return object;
}

/**
 * Frees a given allocated object.
 */
static void freeObject(FalconObj *object) {
    switch (object->type) {
        case OBJ_STRING: {
            FalconObjString *string = (FalconObjString *) object;
            FalconReallocate(object, sizeof(FalconObjString) + string->length + 1, 0);
            break;
        }
        case OBJ_UPVALUE:
            FALCON_FREE(FalconObjUpvalue, object);
            break;
        case OBJ_CLOSURE: {
            FalconObjClosure *closure = (FalconObjClosure *) object;
            FALCON_FREE_ARRAY(FalconObjUpvalue *, closure->upvalues, closure->upvalueCount);
            FALCON_FREE(FalconObjClosure, object);
            break;
        }
        case OBJ_FUNCTION: {
            FalconObjFunction *function = (FalconObjFunction *) object;
            FalconFreeBytecode(&function->bytecodeChunk);
            FALCON_FREE(FalconObjFunction, object);
            break;
        }
        case OBJ_NATIVE:
            FALCON_FREE(FalconObjNative, object);
            break;
    }
}

/**
 * Frees all the objects allocated in the virtual machine.
 */
void FalconFreeObjects(VM *vm) {
    FalconObj *object = vm->objects;
    while (object != NULL) {
        FalconObj *next = object->next;
        freeObject(object);
        object = next;
    }
}
