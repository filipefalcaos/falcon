/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * falcon_memory.c: Falcon's automatic memory manager
 * See Falcon's license in the LICENSE file
 */

#include "falcon_memory.h"
#include "../compiler/falcon_compiler.h"
#include <stdio.h>
#include <stdlib.h>

#ifdef FALCON_DEBUG_LOG_GC
#include <stdio.h>
#include "../lib/falcon_debug.h"
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
 * Marks all root objects in the VM. Root objects are any object that the VM can reach directly,
 * mostly global variables or objects on the stack.
 */
static void markRootsGC(FalconVM *vm) {
    for (FalconValue *slot = vm->stack; slot < vm->stackTop; slot++) {
        FalconMarkValue(vm, *slot); /* Marks stack objects */
    }

    for (int i = 0; i < vm->frameCount; i++) {
        FalconMarkObject(vm, (FalconObj *) vm->frames[i].closure); /* Marks closure objects */
    }

    for (FalconObjUpvalue *upvalue = vm->openUpvalues; upvalue != NULL; upvalue = upvalue->next) {
        FalconMarkObject(vm, (FalconObj *) upvalue); /* Marks open upvalues */
    }

    FalconMarkTable(vm, &vm->globals); /* Marks global variables */
    FalconMarkCompilerRoots(vm);       /* Marks compilation roots */
}

/**
 * Traces the references of the "grey" objects while there are still "grey" objects to trace. After
 * being traced, a "grey" object is turned "black".
 */
static void traceReferencesGC(FalconVM *vm) {
    while (vm->grayCount > 0) {
        FalconObj *object = vm->grayStack[--vm->grayCount];
        FalconBlackenObject(vm, object);
    }
}

/**
 * Starts an immediate garbage collection procedure to free unused memory. Garbage collection
 * follows the Mark-sweep algorithm. It consists in two steps:
 *
 * - Marking: starting from the roots (any object that the VM can reach directly), trace all of the
 * objects those roots refer to. Each time an object is visited, it is marked.
 * - Sweeping: every object traced is examined: any unmarked object are unreachable, thus garbage,
 * and is freed.
 */
void FalconRunGC(FalconVM *vm) {
#ifdef FALCON_DEBUG_LOG_GC
    printf("== Gargabe Collector Start ==\n");
#endif

    markRootsGC(vm);       /* Marks all roots */
    traceReferencesGC(vm); /* Traces the references of the "grey" objects */

#ifdef FALCON_DEBUG_LOG_GC
    printf("== Gargabe Collector End ==\n");
#endif
}

/**
 * Handles all dynamic memory management — allocating memory, freeing it, and changing the size of
 * an existing allocation.
 */
void *FalconReallocate(FalconVM *vm, void *previous, size_t oldSize, size_t newSize) {
    if (newSize > oldSize) { /* More memory allocation? */
#ifdef FALCON_DEBUG_STRESS_GC
        FalconRunGC(vm);
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

#ifdef FALCON_DEBUG_LOG_GC
    printf("%p allocate %ld for type %d\n", (void *) object, size, type);
#endif

    return object;
}

/**
 * Frees a given allocated object.
 */
static void freeObject(FalconVM *vm, FalconObj *object) {
#ifdef FALCON_DEBUG_LOG_GC
    printf("%p free type %d\n", (void *) object, object->type);
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
