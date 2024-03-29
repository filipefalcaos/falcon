/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * fl-mem.c: Falcon's automatic memory manager
 * See Falcon's license in the LICENSE file
 */

#include "fl-mem.h"
#include "fl-debug.h"
#include <stdio.h>
#include <stdlib.h>

/**
 * Prints a message to stderr indicating that a memory allocation error occurred.
 */
void memory_error() {
    fprintf(stderr, FALCON_OUT_OF_MEM_ERR);
    fprintf(stderr, "\n");
    exit(FALCON_ERR_MEMORY);
}

/**
 * Handles all dynamic memory management: allocating memory, freeing it, and changing the size of
 * an existing allocation. It follows the logic:
 * - If oldSize < newSize, shrinks the memory allocated;
 * - If newSize > oldSize, increases the memory allocated (garbage collector run may be triggered
 * before the new allocation);
 * - If newSize == 0, frees the memory allocated.
 */
void *reallocate(FalconVM *vm, void *previous, size_t oldSize, size_t newSize) {
    vm->bytesAllocated += newSize - oldSize;

    if ((newSize > oldSize) && vm->gcEnabled) {
#ifdef FALCON_STRESS_GC
        run_GC(vm); /* Runs the garbage collector always */
#else
        if (vm->bytesAllocated > vm->nextGC) run_GC(vm); /* Runs the garbage collector, if needed */
#endif
    }

    if (newSize == 0) { /* Deallocate memory? */
        free(previous);
        return NULL;
    }

    void *allocatedMemory = realloc(previous, newSize); /* Reallocates memory */
    if (allocatedMemory == NULL) {                      /* Allocation failed? */
        run_GC(vm); /* Runs the garbage collector to free some memory */
        allocatedMemory = realloc(previous, newSize); /* Retries to reallocate memory */

        if (allocatedMemory == NULL) /* Allocation still failed? */
            memory_error();          /* No memory available, exit */
    }

    return allocatedMemory;
}

/**
 * Allocates a new FalconObj with a given size and initializes the object fields, also adding it to
 * the list of objects in the virtual machine.
 */
FalconObj *allocate_object(FalconVM *vm, size_t size, ObjType type) {
    FalconObj *object = (FalconObj *) reallocate(vm, NULL, 0, size); /* Sets a new object */
    object->isMarked = false;
    object->type = type;        /* Sets the object type */
    object->next = vm->objects; /* Adds the new object to the objects list */
    vm->objects = object;

    if (vm->traceMemory) dump_allocation(object, size, type);
    return object;
}

/**
 * Frees the memory previously allocated to a given FalconObj.
 */
void free_object(FalconVM *vm, FalconObj *object) {
    if (vm->traceMemory) dump_free(object);

    switch (object->type) {
        case OBJ_STRING: {
            ObjString *string = (ObjString *) object;
            reallocate(vm, object, sizeof(ObjString) + string->length + 1, 0);
            break;
        }
        case OBJ_FUNCTION: {
            ObjFunction *function = (ObjFunction *) object;
            free_bytecode(vm, &function->bytecode);
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
        case OBJ_CLASS: {
            ObjClass *class_ = (ObjClass *) object;
            free_map(vm, &class_->methods);
            FALCON_FREE(vm, ObjClass, object);
            break;
        }
        case OBJ_INSTANCE: {
            ObjInstance *instance = (ObjInstance *) object;
            free_map(vm, &instance->fields);
            FALCON_FREE(vm, ObjInstance, object);
            break;
        }
        case OBJ_BMETHOD:
            FALCON_FREE(vm, ObjBMethod, object);
            break;
        case OBJ_LIST: {
            ObjList *list = (ObjList *) object;
            FALCON_FREE_ARRAY(vm, FalconValue, list->elements.values, list->elements.capacity);
            FALCON_FREE(vm, ObjList, object);
            break;
        }
        case OBJ_MAP:
            free_map(vm, (ObjMap *) object);
            break;
        case OBJ_NATIVE:
            FALCON_FREE(vm, ObjNative, object);
            break;
    }
}

/**
 * Frees the memory previously allocated to all FalconObjs allocated by the virtual machine.
 */
void free_vm_objects(FalconVM *vm) {
    FalconObj *object = vm->objects;
    while (object != NULL) {
        FalconObj *next = object->next;
        free_object(vm, object);
        object = next;
    }

    free(vm->grayStack); /* Frees the GC's gray stack */
}
