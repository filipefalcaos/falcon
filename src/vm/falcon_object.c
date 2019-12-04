/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * falcon_object.h: Falcon's object representation
 * See Falcon's license in the LICENSE file
 */

#include "falcon_object.h"
#include "falcon_memory.h"
#include <stdio.h>

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
        FALCON_ALLOCATE(FalconObjUpvalue *, function->upvalueCount); /* Sets upvalue list */

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
FalconObjNative *FalconNewNative(FalconVM *vm, FalconNativeFn function) {
    FalconObjNative *native = FALCON_ALLOCATE_OBJ(vm, FalconObjNative, FALCON_OBJ_NATIVE);
    native->function = function;
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
