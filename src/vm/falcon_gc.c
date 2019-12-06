/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * falcon_gc.c: Falcon's garbage collector
 * See Falcon's license in the LICENSE file
 */

#include "falcon_gc.h"
#include "falcon_memory.h"
#include <stdlib.h>

#ifdef FALCON_DEBUG_LOG_GC
#include <stdio.h>
#endif

/* The grow factor for the heap allocation. It follows the logic:
 * - If factor > 1, then the number of bytes required for the next garbage collector will be higher
 * than the current number of allocated bytes
 * - If factor < 1, then the number of bytes required for the next garbage collector will be lower
 * - If factor = 1, then the number of bytes required for the next garbage collector will be the
 * current number of allocated bytes */
#define FALCON_HEAP_GROW_FACTOR 2

/**
 * Marks a Falcon Object for garbage collection.
 */
static void markObject(FalconVM *vm, FalconObj *object) {
    if (object == NULL) return;
    if (object->isMarked) return;

    object->isMarked = true;
    if (object->type == FALCON_OBJ_NATIVE || object->type == FALCON_OBJ_STRING)
        return; /* Strings and native functions contain no references to trace */

#ifdef FALCON_DEBUG_LOG_GC
    printf("%p marked ", (void *) object);
    FalconPrintValue(FALCON_OBJ_VAL(object));
    printf("\n");
#endif

    if (vm->grayCapacity < vm->grayCount + 1) {
        vm->grayCapacity = FALCON_INCREASE_CAPACITY(vm->grayCapacity); /* Increase the capacity */
        vm->grayStack = realloc(vm->grayStack, sizeof(FalconObj *) * vm->grayCapacity);
    }

    vm->grayStack[vm->grayCount++] = object; /* Adds to the GC worklist */
}

/**
 * Marks compilation roots (the FalconObjFunction the compiler is compiling into) for garbage
 * collection.
 */
static void markCompilerRoots(FalconVM *vm) {
    FalconFunctionCompiler *compiler = vm->compiler;
    while (compiler != NULL) {
        markObject(vm, (FalconObj *) compiler->function);
        compiler = compiler->enclosing;
    }
}

/**
 * Marks a Falcon Value for garbage collection.
 */
static void markValue(FalconVM *vm, FalconValue value) {
    if (!FALCON_IS_OBJ(value)) return; /* Num, bool, and null are not dynamically allocated */
    markObject(vm, FALCON_AS_OBJ(value));
}

/**
 * Marks every key/value pair in the hashtable for garbage collection.
 */
static void markTable(FalconVM *vm, FalconTable *table) {
    for (int i = 0; i < table->capacity; i++) {
        Entry *entry = &table->entries[i];
        markObject(vm, (FalconObj *) entry->key);
        markValue(vm, entry->value);
    }
}

/**
 * Marks all Falcon Values in a value array for garbage collection.
 */
static void markArray(FalconVM *vm, FalconValueArray *array) {
    for (int i = 0; i < array->count; i++) {
        markValue(vm, array->values[i]);
    }
}

/**
 * Marks all the captured upvalues of a closure for garbage collection.
 */
static void markUpvalues(FalconVM *vm, FalconObjClosure *closure) {
    for (int i = 0; i < closure->upvalueCount; i++) {
        markObject(vm, (FalconObj *) closure->upvalues[i]);
    }
}

/**
 * Traces all the references of a Falcon Object, marking the "grey", and turns the object "black"
 * for the garbage collection. "Black" objects are the ones with "isMarked" set to true and that
 * are not in the "grey" stack.
 */
static void blackenObject(FalconVM *vm, FalconObj *object) {
#ifdef FALCON_DEBUG_LOG_GC
    printf("%p blackened ", (void *) object);
    FalconPrintValue(FALCON_OBJ_VAL(object));
    printf("\n");
#endif

    switch (object->type) {
        case FALCON_OBJ_CLOSURE: {
            FalconObjClosure *closure = (FalconObjClosure *) object;
            markObject(vm, (FalconObj *) closure->function);
            markUpvalues(vm, closure);
            break;
        }
        case FALCON_OBJ_FUNCTION: {
            FalconObjFunction *function = (FalconObjFunction *) object;
            markObject(vm, (FalconObj *) function->name);
            markArray(vm, &function->bytecodeChunk.constants);
            break;
        }
        case FALCON_OBJ_UPVALUE:
            markValue(vm, ((FalconObjUpvalue *) object)->closed);
            break;
        case FALCON_OBJ_NATIVE:
        case FALCON_OBJ_STRING:
            break;
    }
}

/**
 * Marks all root objects in the VM. Root objects are any object that the VM can reach directly,
 * mostly global variables or objects on the stack.
 */
static void markRootsGC(FalconVM *vm) {
    for (FalconValue *slot = vm->stack; slot < vm->stackTop; slot++) {
        markValue(vm, *slot); /* Marks stack objects */
    }

    for (int i = 0; i < vm->frameCount; i++) {
        markObject(vm, (FalconObj *) vm->frames[i].closure); /* Marks closure objects */
    }

    for (FalconObjUpvalue *upvalue = vm->openUpvalues; upvalue != NULL; upvalue = upvalue->next) {
        markObject(vm, (FalconObj *) upvalue); /* Marks open upvalues */
    }

    markTable(vm, &vm->globals); /* Marks global variables */
    markCompilerRoots(vm);       /* Marks compilation roots */
}

/**
 * Traces the references of the "grey" objects while there are still "grey" objects to trace. After
 * being traced, a "grey" object is turned "black".
 */
static void traceReferencesGC(FalconVM *vm) {
    while (vm->grayCount > 0) {
        FalconObj *object = vm->grayStack[--vm->grayCount];
        blackenObject(vm, object);
    }
}

/**
 * Removes every key/value pair of "white" objects in the hashtable for garbage collection.
 */
static void removeWhitesTable(FalconTable *table) {
    for (int i = 0; i < table->capacity; i++) {
        Entry *current = &table->entries[i];
        if (current->key != NULL && !current->key->obj.isMarked) /* Is a "white" object? */
            FalconTableDelete(table, current->key); /* Removes key/value pair from the table */
    }
}

/**
 * Traverses the list of objects in the VM looking for the "white" ones. When found, "white"
 * objects are removed from the list and freed.
 */
static void sweepGC(FalconVM *vm) {
    FalconObj *previous = NULL;
    FalconObj *current = vm->objects;

    while (current != NULL) {
        if (current->isMarked) {       /* Is the object marked? If so, keep it */
            current->isMarked = false; /* Remove the mark for the next GC */
            previous = current;
            current = previous->next;
        } else { /* Unmarked objects - the "white" ones */
            FalconObj *white = current;
            current = current->next;

            /* Removes the "white" object from the list */
            if (previous != NULL) {
                previous->next = current;
            } else {
                vm->objects = current;
            }

            freeObject(vm, white); /* Frees the "white" object */
        }
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
    printf("== Garbage Collector Start ==\n");
    size_t bytesAllocated = vm->bytesAllocated;
#endif

    markRootsGC(vm);                 /* Marks all roots */
    traceReferencesGC(vm);           /* Traces the references of the "grey" objects */
    removeWhitesTable(&vm->strings); /* Removes the "white" strings from the table */
    sweepGC(vm);                     /* Reclaim the "white" objects - garbage */
    vm->nextGC = vm->bytesAllocated * FALCON_HEAP_GROW_FACTOR; /* Adjust the next GC threshold */

#ifdef FALCON_DEBUG_LOG_GC
    printf("Collected %ld bytes (from %ld to %ld)\n", bytesAllocated - vm->bytesAllocated,
           bytesAllocated, vm->bytesAllocated);
    printf("Next GC at %ld bytes\n", vm->nextGC);
    printf("== Garbage Collector End ==\n");
#endif
}
