/*
 * YAPL version 0.0.1 (master, Oct 18 2019)
 * yapl_memory_manager.c: YAPL's automatic memory manager
 * See YAPL's license in the LICENSE file
 */

#include "yapl_memmanager.h"
#include "../lib/yapl_natives.h"
#include <stdio.h>
#include <stdlib.h>

/**
 * Presents a message indicating that a memory allocation error occurred.
 */
void memoryError() {
    fprintf(stderr, OUT_OF_MEM_ERR);
    fprintf(stderr, "\n");
    exit(1);
}

/**
 * Handles all dynamic memory management — allocating memory, freeing it, and changing the size of
 * an existing allocation.
 */
void *reallocate(void *previous, size_t oldSize, size_t newSize) {
    if (newSize == 0) {
        free(previous);
        return NULL;
    }

    return realloc(previous, newSize);
}

/**
 * Allocates a new YAPL object.
 */
Obj *allocateObject(VM *vm, size_t size, ObjType type) {
    Obj *object = (Obj *) reallocate(NULL, 0, size); /* Creates a new object */

    if (object == NULL) { /* Checks if the allocation failed */
        memoryError();
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
static void freeObject(Obj *object) {
    switch (object->type) {
        case OBJ_STRING: {
            ObjString *string = (ObjString *) object;
            reallocate(object, sizeof(ObjString) + string->length + 1, 0);
            break;
        }
        case OBJ_UPVALUE:
            FREE(ObjUpvalue, object);
            break;
        case OBJ_CLOSURE: {
            ObjClosure *closure = (ObjClosure *) object;
            FREE_ARRAY(ObjUpvalue *, closure->upvalues, closure->upvalueCount);
            FREE(ObjClosure, object);
            break;
        }
        case OBJ_FUNCTION: {
            ObjFunction *function = (ObjFunction *) object;
            freeBytecodeChunk(&function->bytecodeChunk);
            FREE(ObjFunction, object);
            break;
        }
        case OBJ_NATIVE:
            FREE(ObjNative, object);
            break;
    }
}

/**
 * Frees all the objects allocated in the virtual machine.
 */
void freeObjects(VM *vm) {
    Obj *object = vm->objects;
    while (object != NULL) {
        Obj *next = object->next;
        freeObject(object);
        object = next;
    }
}
