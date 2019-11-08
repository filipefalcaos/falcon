/*
 * YAPL version 0.0.1 (master, Oct 18 2019)
 * yapl_object.c: YAPL's object representation
 * See YAPL's license in the LICENSE file
 */

#include "yapl_object.h"
#include "../yapl.h"
#include <stdio.h>

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
            printf("%s", AS_CLANG_STRING(value));
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
