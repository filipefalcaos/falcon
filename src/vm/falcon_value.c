/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * falcon_value.c: Falcon's value representation
 * See Falcon's license in the LICENSE file
 */

#include "falcon_value.h"
#include "falcon_memory.h"
#include <stdio.h>
#include <string.h>

/**
 * Initializes an empty ValueArray.
 */
void falconInitValArray(ValueArray *valueArray) {
    valueArray->count = 0;
    valueArray->capacity = 0;
    valueArray->values = NULL;
}

/**
 * Frees a ValueArray.
 */
void falconFreeValArray(FalconVM *vm, ValueArray *valueArray) {
    FALCON_FREE_ARRAY(vm, FalconValue, valueArray->values, valueArray->capacity);
    falconInitValArray(valueArray);
}

/**
 * Appends a Value to the end of a ValueArray. If the current size is not enough, the capacity of
 * the array is increased to fit the new Value.
 */
void falconWriteValArray(FalconVM *vm, ValueArray *valueArray, FalconValue value) {
    if (valueArray->capacity < valueArray->count + 1) {
        int oldCapacity = valueArray->capacity;
        valueArray->capacity = FALCON_INCREASE_CAPACITY(oldCapacity);
        valueArray->values = FALCON_INCREASE_ARRAY(vm, valueArray->values, FalconValue, oldCapacity,
                                                   valueArray->capacity);
    }

    valueArray->values[valueArray->count] = value;
    valueArray->count++;
}

/**
 * Checks if two Falcon Values are equal.
 */
bool falconValEqual(FalconValue a, FalconValue b) {
    if (a.type != b.type) return false;

    switch (a.type) {
        case VAL_BOOL:
            return FALCON_AS_BOOL(a) == FALCON_AS_BOOL(b);
        case VAL_NULL:
            return true;
        case VAL_NUM:
            return FALCON_AS_NUM(a) == FALCON_AS_NUM(b);
        case VAL_OBJ:
            return FALCON_AS_OBJ(a) == FALCON_AS_OBJ(b);
        default:
            return false;
    }
}

/**
 * Takes the logical not of a value. In Falcon, 'null', 'false', the number zero, and an empty
 * string are falsy, while every other value behaves like 'true'.
 */
bool falconIsFalsy(FalconValue value) {
    return FALCON_IS_NULL(value) || (FALCON_IS_BOOL(value) && !FALCON_AS_BOOL(value)) ||
           (FALCON_IS_NUM(value) && FALCON_AS_NUM(value) == 0) ||
           (FALCON_IS_STRING(value) && FALCON_AS_STRING(value)->length == 0);
}

/* String conversion constants */
#define MAX_NUM_TO_STR       24
#define NUM_TO_STR_FORMATTER "%.14g"

/* Converts a function to a string */
#define FUNCTION_TO_STR(vm, function)                                         \
    do {                                                                      \
        if ((function)->name == NULL) {                                       \
            string = FALCON_SCRIPT;                                           \
        } else {                                                              \
            string = FALCON_ALLOCATE(vm, char, (function)->name->length + 6); \
            sprintf(string, "<fn %s>", (function)->name->chars);              \
        }                                                                     \
    } while (false)

/**
 * Converts a given Falcon Value to a Falcon string.
 */
char *falconValToString(FalconVM *vm, FalconValue *value) {
    char *string = NULL;

    switch (value->type) {
        case VAL_BOOL:
            return (FALCON_AS_BOOL(*value) ? "true" : "false");
        case VAL_NULL:
            return "null";
        case VAL_NUM:
            string = FALCON_ALLOCATE(vm, char, MAX_NUM_TO_STR);
            sprintf(string, NUM_TO_STR_FORMATTER, FALCON_AS_NUM(*value));
            break;
        case VAL_OBJ:
            switch (FALCON_OBJ_TYPE(*value)) {
                case OBJ_STRING:
                    return FALCON_AS_CSTRING(*value);
                case OBJ_CLOSURE: {
                    ObjClosure *closure = FALCON_AS_CLOSURE(*value);
                    FUNCTION_TO_STR(vm, closure->function);
                    break;
                }
                case OBJ_FUNCTION: {
                    ObjFunction *function = FALCON_AS_FUNCTION(*value);
                    FUNCTION_TO_STR(vm, function);
                    break;
                }
                case OBJ_NATIVE: {
                    ObjNative *native = FALCON_AS_NATIVE(*value);
                    string = FALCON_ALLOCATE(vm, char, strlen(native->name) + 13);
                    sprintf(string, "<native fn %s>", native->name);
                    break;
                }
                case OBJ_LIST:
                    (void) *value; /* TODO: implement list to string conversion */
                    break;
                default:
                    break;
            }
        default:
            break;
    }

    return string; /* Returns the string */
}

#undef MAX_NUM_TO_STR
#undef NUM_TO_STR_FORMATTER
#undef FUNCTION_TO_STR

/**
 * Prints a single Falcon Value.
 */
void falconPrintVal(FalconVM *vm, FalconValue value) {
    switch (value.type) {
        case VAL_NUM:
            printf("%g", FALCON_AS_NUM(value));
            break;
        default:
            printf("%s", falconValToString(vm, &value));
            break;
    }
}
