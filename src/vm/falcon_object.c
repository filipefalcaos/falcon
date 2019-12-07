/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * falcon_object.h: Falcon's object representation
 * See Falcon's license in the LICENSE file
 */

#include "falcon_object.h"
#include "falcon_memory.h"
#include <stdio.h>

/**
 * Gets the name (string) of a given Falcon Object type.
 */
const char *falconGetObjName(ObjType type) {
    const char *objectTypeNames[] = {"OBJ_STRING", "OBJ_UPVALUE",
                                     "OBJ_CLOSURE", "OBJ_FUNCTION",
                                     "OBJ_NATIVE"};
    return objectTypeNames[type];
}

/**
 * Allocates a new Falcon upvalue object.
 */
ObjUpvalue *falconUpvalue(FalconVM *vm, FalconValue *slot) {
    ObjUpvalue *upvalue = FALCON_ALLOCATE_OBJ(vm, ObjUpvalue, OBJ_UPVALUE);
    upvalue->slot = slot;
    upvalue->next = NULL;
    upvalue->closed = FALCON_NULL_VAL;
    return upvalue;
}

/**
 * Allocates a new Falcon closure object.
 */
ObjClosure *falconClosure(FalconVM *vm, ObjFunction *function) {
    ObjUpvalue **upvalues =
        FALCON_ALLOCATE(vm, ObjUpvalue *, function->upvalueCount); /* Sets upvalue list */

    for (int i = 0; i < function->upvalueCount; i++) {
        upvalues[i] = NULL; /* Initialize current upvalue */
    }

    ObjClosure *closure = FALCON_ALLOCATE_OBJ(vm, ObjClosure, OBJ_CLOSURE);
    closure->function = function;
    closure->upvalues = upvalues;
    closure->upvalueCount = function->upvalueCount;
    return closure;
}

/**
 * Allocates a new Falcon function object.
 */
ObjFunction *falconFunction(FalconVM *vm) {
    ObjFunction *function = FALCON_ALLOCATE_OBJ(vm, ObjFunction, OBJ_FUNCTION);
    function->arity = 0;
    function->upvalueCount = 0;
    function->name = NULL;
    falconInitBytecode(&function->bytecode);
    return function;
}

/**
 * Allocates a new Falcon native function object.
 */
ObjNative *falconNative(FalconVM *vm, FalconNativeFn function, const char *name) {
    ObjNative *native = FALCON_ALLOCATE_OBJ(vm, ObjNative, OBJ_NATIVE);
    native->function = function;
    native->name = name;
    return native;
}

/**
 * Prints a Falcon function name.
 */
static void printFunction(ObjFunction *function) {
    if (function->name == NULL) { /* Checks if in top level code */
        printf(FALCON_SCRIPT);
        return;
    }
    printf("<fn %s>", function->name->chars);
}

/**
 * Prints a single Falcon object.
 */
void falconPrintObj(FalconValue value) {
    switch (FALCON_OBJ_TYPE(value)) {
        case OBJ_STRING:
            printf("%s", FALCON_AS_CSTRING(value));
            break;
        case OBJ_UPVALUE:
            break; /* Upvalues cannot be printed */
        case OBJ_CLOSURE:
            printFunction(FALCON_AS_CLOSURE(value)->function);
            break;
        case OBJ_FUNCTION:
            printFunction(FALCON_AS_FUNCTION(value));
            break;
        case OBJ_NATIVE:
            printf("<native fn>");
            break;
    }
}
