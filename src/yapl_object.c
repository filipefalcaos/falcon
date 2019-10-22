/*
 * YAPL version 0.0.1 (master, Oct 18 2019)
 * yapl_object.c: YAPL's object representation
 * See YAPL's license in the LICENSE file
 */

#include "../include/yapl_object.h"
#include "../include/yapl_memory_manager.h"
#include "../include/yapl_value.h"
#include "../include/yapl_vm.h"
#include <stdio.h>
#include <string.h>

/* Allocates an object of a given type */
#define ALLOCATE_OBJ(type, objectType) (type *) allocateObject(sizeof(type), objectType)

/**
 * Prints a YAPL function name.
 */
static void printFunction(ObjFunction *function) {
    if (function->name == NULL) { /* Checks if in top level code */
        printf("<script>");
        return;
    }
    printf("<fn %s>", function->name->chars);
}

/**
 * Prints a single YAPL object.
 */
void printObject(Value value) {
    switch (OBJ_TYPE(value)) {
        case OBJ_FUNCTION:
            printFunction(AS_FUNCTION(value));
            break;
        case OBJ_NATIVE:
            printf("<native fn>");
            break;
        case OBJ_STRING:
            printf("\"%s\"", AS_CLANG_STRING(value));
            break;
    }
}

/**
 * Allocates a new YAPL object.
 */
Obj *allocateObject(size_t size, ObjType type) {
    Obj *object = (Obj *) reallocate(NULL, 0, size); /* Creates a new object */
    object->type = type;                             /* Sets the object type */
    object->next = vm.objects;                       /* Adds the new object to the object list */
    vm.objects = object;
    return object;
}

/**
 * Allocates a new YAPL function object.
 */
ObjFunction *newFunction() {
    ObjFunction *function = ALLOCATE_OBJ(ObjFunction, OBJ_FUNCTION);
    function->arity = 0;
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

/**
 * Hashes an input string using FNV-1a hash function.
 */
uint32_t hashString(const char *key, int length) {
    uint32_t hash = 2166136261u;

    for (int i = 0; i < length; i++) {
        hash ^= key[i];
        hash *= 16777619;
    }

    return hash;
}

/**
 * Creates a new ObjString by claiming ownership of the given string. In this case, the characters
 * of a ObjString can be freed when no longer needed.
 */
ObjString *makeString(int length) {
    ObjString *string = (ObjString *) allocateObject(sizeof(ObjString) + length + 1, OBJ_STRING);
    string->length = length;
    return string;
}

/**
 * Copies and allocates a given string to the heap. This way, every ObjString reliably owns its
 * character array and can free it.
 */
ObjString *copyString(const char *chars, int length) {
    uint32_t hash = hashString(chars, length);
    ObjString *interned = tableFindStr(&vm.strings, chars, length, hash); /* Checks if interned */
    if (interned != NULL) return interned;

    ObjString *string = makeString(length);
    memcpy(string->chars, chars, length);
    string->chars[length] = '\0';
    string->hash = hash;
    tableSet(&vm.strings, string, NULL_VAL); /* Intern the string */
    return string;
}
