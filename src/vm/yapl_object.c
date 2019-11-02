/*
 * YAPL version 0.0.1 (master, Oct 18 2019)
 * yapl_object.c: YAPL's object representation
 * See YAPL's license in the LICENSE file
 */

#include "yapl_object.h"
#include "yapl_memmanager.h"
#include "yapl_value.h"
#include "yapl_vm.h"
#include <stdio.h>

/* Allocates an object of a given type */
#define ALLOCATE_OBJ(type, objectType) (type *) allocateObject(sizeof(type), objectType)

/**
 * Prints a YAPL function name.
 */
static void printFunction(ObjFunction *function) {
    if (function->name == NULL) { /* Checks if in top level code */
        printf(SCRIPT_TAG);
        return;
    }
    printf("<fn %s>", function->name->chars);
}

/**
 * Prints a single YAPL object.
 */
void printObject(Value value) {
    switch (OBJ_TYPE(value)) {
        case OBJ_STRING:
            printf("\"%s\"", AS_CLANG_STRING(value));
            break;
        case OBJ_UPVALUE:
            break; /* Upvalues cannot be printed */
        case OBJ_CLOSURE:
            printFunction(AS_CLOSURE(value)->function);
            break;
        case OBJ_FUNCTION:
            printFunction(AS_FUNCTION(value));
            break;
        case OBJ_NATIVE:
            printf("<native fn>");
            break;
    }
}

/**
 * Allocates a new YAPL object.
 */
Obj *allocateObject(size_t size, ObjType type) {
    Obj *object = (Obj *) reallocate(NULL, 0, size); /* Creates a new object */

    if (object == NULL) { /* Checks if the allocation failed */
        memoryError();
        return NULL;
    }

    object->type = type;       /* Sets the object type */
    object->next = vm.objects; /* Adds the new object to the object list */
    vm.objects = object;
    return object;
}

/**
 * Allocates a new YAPL upvalue object.
 */
ObjUpvalue *newUpvalue(Value *slot) {
    ObjUpvalue *upvalue = ALLOCATE_OBJ(ObjUpvalue, OBJ_UPVALUE);
    upvalue->slot = slot;
    upvalue->next = NULL;
    upvalue->closed = NULL_VAL;
    return upvalue;
}

/**
 * Allocates a new YAPL closure object.
 */
ObjClosure *newClosure(ObjFunction *function) {
    ObjUpvalue **upvalues = ALLOCATE(ObjUpvalue *, function->upvalueCount); /* Sets upvalue list */

    for (int i = 0; i < function->upvalueCount; i++) {
        upvalues[i] = NULL; /* Initialize current upvalue */
    }

    ObjClosure *closure = ALLOCATE_OBJ(ObjClosure, OBJ_CLOSURE);
    closure->function = function;
    closure->upvalues = upvalues;
    closure->upvalueCount = function->upvalueCount;
    return closure;
}

/**
 * Allocates a new YAPL function object.
 */
ObjFunction *newFunction() {
    ObjFunction *function = ALLOCATE_OBJ(ObjFunction, OBJ_FUNCTION);
    function->arity = 0;
    function->upvalueCount = 0;
    function->name = NULL;
    initBytecodeChunk(&function->bytecodeChunk);
    return function;
}

/**
 * Allocates a new YAPL native function object.
 */
ObjNative *newNative(NativeFn function) {
    ObjNative *native = ALLOCATE_OBJ(ObjNative, OBJ_NATIVE);
    native->function = function;
    return native;
}
