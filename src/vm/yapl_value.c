/*
 * YAPL version 0.0.1 (master, Oct 18 2019)
 * yapl_value.c: YAPL's value representation
 * See YAPL's license in the LICENSE file
 */

#include "yapl_value.h"
#include "yapl_memmanager.h"
#include <stdio.h>
#include <string.h>

/**
 * Initializes an empty ValueArray.
 */
void initValueArray(ValueArray *valueArray) {
    valueArray->count = 0;
    valueArray->capacity = 0;
    valueArray->values = NULL;
}

/**
 * Frees a ValueArray.
 */
void freeValueArray(ValueArray *valueArray) {
    FREE_ARRAY(Value, valueArray->values, valueArray->capacity);
    initValueArray(valueArray);
}

/**
 * Appends a Value to the end of a ValueArray. If the current size is not enough, the capacity of
 * the array is increased to fit the new Value.
 */
void writeValueArray(ValueArray *valueArray, Value value) {
    if (valueArray->capacity < valueArray->count + 1) {
        int oldCapacity = valueArray->capacity;
        valueArray->capacity = INCREASE_CAPACITY(oldCapacity);
        valueArray->values =
            INCREASE_ARRAY(valueArray->values, Value, oldCapacity, valueArray->capacity);

        if (valueArray->values == NULL) { /* Checks if the allocation failed */
            memoryError();
            return;
        }
    }

    valueArray->values[valueArray->count] = value;
    valueArray->count++;
}

/**
 * Prints a single YAPL Value.
 */
void printValue(Value value) {
    switch (value.type) {
        case VAL_BOOL:
            printf(AS_BOOL(value) ? "true" : "false");
            break;
        case VAL_NULL:
            printf("null");
            break;
        case VAL_NUM:
            printf("%g", AS_NUM(value));
            break;
        case VAL_OBJ:
            printObject(value);
            break;
        default:
            break;
    }
}

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

/**
 * Checks if two YAPL Values are equal.
 */
bool valuesEqual(Value a, Value b) {
    if (a.type != b.type) return false;

    switch (a.type) {
        case VAL_BOOL:
            return AS_BOOL(a) == AS_BOOL(b);
        case VAL_NULL:
            return true;
        case VAL_NUM:
            return AS_NUM(a) == AS_NUM(b);
        case VAL_OBJ:
            return AS_OBJ(a) == AS_OBJ(b);
        default:
            return false;
    }
}

/**
 * Takes the logical not (falsiness) of a value. In YAPL, 'null', 'false', the number zero, and an empty string are
 * falsey, while every other value behaves like 'true'.
 */
bool isFalsey(Value value) {
    return IS_NULL(value) || (IS_BOOL(value) && !AS_BOOL(value)) || (IS_NUM(value) && AS_NUM(value) == 0) ||
           (IS_STRING(value) && AS_STRING(value)->length == 0);
}

/* String conversion constants */
#define MAX_NUM_TO_STR       50
#define NUM_TO_STR_FORMATTER "%.14g"

/**
 * Converts a given YAPL Value to a YAPL string.
 */
char *valueToString(Value *value) {
    char *string = NULL;

    switch (value->type) {
        case VAL_BOOL:
            string = (AS_BOOL(*value) ? "true" : "false");
            break;
        case VAL_NULL:
            string = "null";
            break;
        case VAL_NUM:
            string = ALLOCATE(char, MAX_NUM_TO_STR + 1);
            sprintf(string, NUM_TO_STR_FORMATTER, AS_NUM(*value)); /* TODO: increase accuracy */
            break;
        case VAL_OBJ:
            switch (OBJ_TYPE(*value)) {
                case OBJ_CLOSURE: /* TODO: add toString support for the objects below */
                    break;
                case OBJ_FUNCTION:
                    break;
                case OBJ_NATIVE:
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }

    return string; /* Returns the string */
}

#undef MAX_NUM_TO_STR
#undef NUM_TO_STR_FORMATTER
