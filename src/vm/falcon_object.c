/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * falcon_object.h: Falcon's object representation
 * See Falcon's license in the LICENSE file
 */

#include "falcon_object.h"
#include "falcon_memory.h"
#include <stdio.h>
#include <stdlib.h>

/**
 * Gets the name (string) of a given Falcon Object type.
 */
const char *getObjectName(FalconObjType type) {
    const char *objectTypeNames[] = {
        "FALCON_OBJ_STRING", "FALCON_OBJ_UPVALUE", "FALCON_OBJ_CLOSURE", "FALCON_OBJ_FUNCTION",
        "FALCON_OBJ_NATIVE"
    };
    return objectTypeNames[type];
}

/**
 * Marks a Falcon Object for garbage collection.
 */
void FalconMarkObject(FalconVM *vm, FalconObj *object) {
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
 * Marks all the captured upvalues of a closure for garbage collection.
 */
static void markUpvalues(FalconVM *vm, FalconObjClosure *closure) {
    for (int i = 0; i < closure->upvalueCount; i++) {
        FalconMarkObject(vm, (FalconObj *) closure->upvalues[i]);
    }
}

/**
 * Traces all the references of a Falcon Object, marking the "grey", and turns the object "black"
 * for the garbage collection. "Black" objects are the ones with "isMarked" set to true and that
 * are not in the "grey" stack.
 */
void FalconBlackenObject(FalconVM *vm, FalconObj *object) {
#ifdef FALCON_DEBUG_LOG_GC
    printf("%p blackened ", (void *) object);
    FalconPrintValue(FALCON_OBJ_VAL(object));
    printf("\n");
#endif

    switch (object->type) {
        case FALCON_OBJ_CLOSURE: {
            FalconObjClosure *closure = (FalconObjClosure *) object;
            FalconMarkObject(vm, (FalconObj *)closure->function);
            markUpvalues(vm, closure);
            break;
        }
        case FALCON_OBJ_FUNCTION: {
            FalconObjFunction *function = (FalconObjFunction *) object;
            FalconMarkObject(vm, (FalconObj *) function->name);
            FalconMarkArray(vm, &function->bytecodeChunk.constants);
            break;
        }
        case FALCON_OBJ_UPVALUE:
            FalconMarkValue(vm, ((FalconObjUpvalue *) object)->closed);
            break;
        case FALCON_OBJ_NATIVE:
        case FALCON_OBJ_STRING:
            break;
    }
}

/**
 * Allocates a new Falcon upvalue object.
 */
FalconObjUpvalue *FalconNewUpvalue(FalconVM *vm, FalconValue *slot) {
    FalconObjUpvalue *upvalue = FALCON_ALLOCATE_OBJ(vm, FalconObjUpvalue, FALCON_OBJ_UPVALUE);
    upvalue->slot = slot;
    upvalue->next = NULL;
    upvalue->closed = FALCON_NULL_VAL;
    return upvalue;
}

/**
 * Allocates a new Falcon closure object.
 */
FalconObjClosure *FalconNewClosure(FalconVM *vm, FalconObjFunction *function) {
    FalconObjUpvalue **upvalues =
        FALCON_ALLOCATE(vm, FalconObjUpvalue *, function->upvalueCount); /* Sets upvalue list */

    for (int i = 0; i < function->upvalueCount; i++) {
        upvalues[i] = NULL; /* Initialize current upvalue */
    }

    FalconObjClosure *closure = FALCON_ALLOCATE_OBJ(vm, FalconObjClosure, FALCON_OBJ_CLOSURE);
    closure->function = function;
    closure->upvalues = upvalues;
    closure->upvalueCount = function->upvalueCount;
    return closure;
}

/**
 * Allocates a new Falcon function object.
 */
FalconObjFunction *FalconNewFunction(FalconVM *vm) {
    FalconObjFunction *function = FALCON_ALLOCATE_OBJ(vm, FalconObjFunction, FALCON_OBJ_FUNCTION);
    function->arity = 0;
    function->upvalueCount = 0;
    function->name = NULL;
    FalconInitBytecode(&function->bytecodeChunk);
    return function;
}

/**
 * Allocates a new Falcon native function object.
 */
FalconObjNative *FalconNewNative(FalconVM *vm, FalconNativeFn function, const char *name) {
    FalconObjNative *native = FALCON_ALLOCATE_OBJ(vm, FalconObjNative, FALCON_OBJ_NATIVE);
    native->function = function;
    native->functionName = name;
    return native;
}

/**
 * Prints a Falcon function name.
 */
static void printFunction(FalconObjFunction *function) {
    if (function->name == NULL) { /* Checks if in top level code */
        printf(FALCON_SCRIPT);
        return;
    }
    printf("<fn %s>", function->name->chars);
}

/**
 * Prints a single Falcon object.
 */
void FalconPrintObject(FalconValue value) {
    switch (FALCON_OBJ_TYPE(value)) {
        case FALCON_OBJ_STRING:
            printf("%s", FALCON_AS_CSTRING(value));
            break;
        case FALCON_OBJ_UPVALUE:
            break; /* Upvalues cannot be printed */
        case FALCON_OBJ_CLOSURE:
            printFunction(FALCON_AS_CLOSURE(value)->function);
            break;
        case FALCON_OBJ_FUNCTION:
            printFunction(FALCON_AS_FUNCTION(value));
            break;
        case FALCON_OBJ_NATIVE:
            printf("<native fn>");
            break;
    }
}
