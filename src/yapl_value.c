/*
 * YAPL version 0.0.1 (master, Oct 18 2019)
 * yapl_value.c: YAPL's value representation
 * See YAPL's license in the LICENSE file
 */

#include "../include/yapl_value.h"
#include "../include/yapl_memory_manager.h"
#include "../include/yapl_object.h"
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
        case VAL_NUMBER:
            printf("%g", AS_NUMBER(value));
            break;
        case VAL_OBJ:
            printObject(value);
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
        case VAL_NUMBER:
            return AS_NUMBER(a) == AS_NUMBER(b);
        case VAL_OBJ: {
            ObjString *aString = AS_STRING(a);
            ObjString *bString = AS_STRING(b);
            return aString->length == bString->length &&
                   memcmp(aString->chars, bString->chars, aString->length) == 0;
        }
    }
}
