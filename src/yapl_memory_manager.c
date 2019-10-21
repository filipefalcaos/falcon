/*
 * YAPL version 0.0.1 (master, Oct 18 2019)
 * yapl_memory_manager.c: YAPL's automatic memory manager
 * See YAPL's license in the LICENSE file
 */

#include <stdlib.h>
#include "../include/commons.h"
#include "../include/yapl_memory_manager.h"
#include "../include/yapl_vm.h"

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
 * Frees a given allocated object.
 */
static void freeObject(Obj *object) {
    switch (object->type) {
        case OBJ_FUNCTION: {
            ObjFunction *function = (ObjFunction *) object;
            freeBytecodeChunk(&function->bytecodeChunk);
            FREE(ObjFunction, object);
            break;
        }
        case OBJ_STRING: {
            ObjString *string = (ObjString *) object;
            reallocate(object, sizeof(ObjString) + string->length + 1, 0);
            break;
        }
    }
}

/**
 * Frees all the objects allocated in the virtual machine.
 */
void freeObjects() {
    Obj *object = vm.objects;
    while (object != NULL) {
        Obj *next = object->next;
        freeObject(object);
        object = next;
    }
}
