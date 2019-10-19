/*
 * YAPL version 0.0.1 (master, Oct 18 2019)
 * yapl_value.h: YAPL's value representation
 * See YAPL's license in the LICENSE file
 */

#ifndef YAPL_VALUE_H
#define YAPL_VALUE_H

#include "commons.h"

/* YAPL's value representation */
typedef double Value;

/* Array of Values */
typedef struct {
    int count;
    int capacity;
    Value *values;
} ValueArray;

/* ValueArray operations */
void initValueArray(ValueArray *valueArray);
void freeValueArray(ValueArray *valueArray);
void writeValueArray(ValueArray *valueArray, Value value);

/* Value operations */
void printValue(Value value);

#endif // YAPL_VALUE_H
