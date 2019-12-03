/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * falcon_value.c: Falcon's value representation
 * See Falcon's license in the LICENSE file
 */

#include "falcon_value.h"
#include "falcon_memory.h"
#include <stdio.h>

/**
 * Initializes an empty ValueArray.
 */
void FalconInitValues(FalconValueArray *valueArray) {
    valueArray->count = 0;
    valueArray->capacity = 0;
    valueArray->values = NULL;
}

/**
 * Frees a ValueArray.
 */
void FalconFreeValues(FalconValueArray *valueArray) {
    FALCON_FREE_ARRAY(FalconValue, valueArray->values, valueArray->capacity);
    FalconInitValues(valueArray);
}

/**
 * Appends a Value to the end of a ValueArray. If the current size is not enough, the capacity of
 * the array is increased to fit the new Value.
 */
void FalconWriteValues(FalconValueArray *valueArray, FalconValue value) {
    if (valueArray->capacity < valueArray->count + 1) {
        int oldCapacity = valueArray->capacity;
        valueArray->capacity = FALCON_INCREASE_CAPACITY(oldCapacity);
        valueArray->values =
            FALCON_INCREASE_ARRAY(valueArray->values, FalconValue, oldCapacity, valueArray->capacity);

        if (valueArray->values == NULL) { /* Checks if the allocation failed */
            FalconMemoryError();
            return;
        }
    }

    valueArray->values[valueArray->count] = value;
    valueArray->count++;
}

/**
 * Prints a single Falcon Value.
 */
void FalconPrintValue(FalconValue value) {
    switch (value.type) {
        case VAL_BOOL:
            printf(FALCON_AS_BOOL(value) ? "true" : "false");
            break;
        case VAL_NULL:
            printf("null");
            break;
        case VAL_NUM:
            printf("%g", FALCON_AS_NUM(value));
            break;
        case VAL_OBJ:
            FalconPrintObject(value);
            break;
        default:
            break;
    }
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

/**
 * Checks if two Falcon Values are equal.
 */
bool FalconValuesEqual(FalconValue a, FalconValue b) {
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
 * Takes the logical not (falsiness) of a value. In Falcon, 'null', 'false', the number zero, and an
 * empty string are falsey, while every other value behaves like 'true'.
 */
bool FalconIsFalsey(FalconValue value) {
    return FALCON_IS_NULL(value) || (FALCON_IS_BOOL(value) && !FALCON_AS_BOOL(value)) ||
           (FALCON_IS_NUM(value) && FALCON_AS_NUM(value) == 0) ||
           (FALCON_IS_STRING(value) && FALCON_AS_STRING(value)->length == 0);
}

/* String conversion constants */
#define FALCON_MAX_NUM_TO_STR       50
#define FALCON_NUM_TO_STR_FORMATTER "%.14g"

/**
 * Converts a given Falcon Value to a Falcon string.
 */
char *FalconValueToString(FalconValue *value) {
    char *string = NULL;

    switch (value->type) {
        case VAL_BOOL:
            string = (FALCON_AS_BOOL(*value) ? "true" : "false");
            break;
        case VAL_NULL:
            string = "null";
            break;
        case VAL_NUM:
            string = FALCON_ALLOCATE(char, FALCON_MAX_NUM_TO_STR + 1);
            sprintf(string, FALCON_NUM_TO_STR_FORMATTER, FALCON_AS_NUM(*value));
            break;
        case VAL_OBJ:
            switch (FALCON_OBJ_TYPE(*value)) {
                case OBJ_CLOSURE: /* TODO: add toString support for the objects below */
                case OBJ_FUNCTION:
                case OBJ_NATIVE:
                default:
                    break;
            }
            break;
        default:
            break;
    }

    return string; /* Returns the string */
}

#undef FALCON_MAX_NUM_TO_STR
#undef FALCON_NUM_TO_STR_FORMATTER
