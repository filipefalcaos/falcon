/*
 * YAPL version 0.0.1 (master, Oct 18 2019)
 * yapl_value.h: YAPL's value representation
 * See YAPL's license in the LICENSE file
 */

#ifndef YAPL_VALUE_H
#define YAPL_VALUE_H

#include "../commons.h"

/* YAPL object types */
typedef struct sObj Obj;
typedef struct sObjString ObjString;

/* Types of values on YAPL */
typedef enum { VAL_BOOL, VAL_NULL, VAL_NUMBER, VAL_OBJ, VAL_ERROR } ValueType;

/* YAPL's value representation */
typedef struct {
    ValueType type;
    union {
        bool boolean;
        double number;
        Obj *obj;
    } as;
} Value;

/* Checks a Value type */
#define IS_BOOL(value)   ((value).type == VAL_BOOL)
#define IS_NULL(value)   ((value).type == VAL_NULL)
#define IS_NUMBER(value) ((value).type == VAL_NUMBER)
#define IS_OBJ(value)    ((value).type == VAL_OBJ)
#define IS_ERROR(value)  ((value).type == VAL_ERROR)

/* Gets the C value from a YAPL Value */
#define AS_BOOL(value)   ((value).as.boolean)
#define AS_NUMBER(value) ((value).as.number)
#define AS_OBJ(value)    ((value).as.obj)

/* Sets a native C value to a YAPL Value */
#define BOOL_VAL(value)   ((Value) {VAL_BOOL, {.boolean = value}})
#define NULL_VAL          ((Value) {VAL_NULL, {.number = 0}})
#define NUMBER_VAL(value) ((Value) {VAL_NUMBER, {.number = value}})
#define OBJ_VAL(object)   ((Value) {VAL_OBJ, {.obj = (Obj*)object}})

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
void printValue(Value value, bool newLine);
bool valuesEqual(Value a, Value b);

#endif // YAPL_VALUE_H
