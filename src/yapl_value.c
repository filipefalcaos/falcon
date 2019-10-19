/*
 * YAPL version 0.0.1 (master, Oct 18 2019)
 * yapl_value.c: YAPL's value representation
 * See YAPL's license in the LICENSE file
 */

#include "../include/yapl_value.h"
#include "../include/yapl_memory_manager.h"
#include <stdio.h>

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
void printValue(Value value) { printf("%g", value); }
