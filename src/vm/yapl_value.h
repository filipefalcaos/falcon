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
typedef enum { VAL_BOOL, VAL_NULL, VAL_NUM, VAL_OBJ, VAL_ERR } ValueType;

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
#define IS_BOOL(value) ((value).type == VAL_BOOL)
#define IS_NULL(value) ((value).type == VAL_NULL)
#define IS_NUM(value)  ((value).type == VAL_NUM)
#define IS_OBJ(value)  ((value).type == VAL_OBJ)
#define IS_ERR(value)  ((value).type == VAL_ERR)

/* Gets the C value from a YAPL Value */
#define AS_BOOL(value) ((value).as.boolean)
#define AS_NUM(value)  ((value).as.number)
#define AS_OBJ(value)  ((value).as.obj)

/* Sets a native C value to a YAPL Value */
#define BOOL_VAL(value) ((Value) {VAL_BOOL, {.boolean = (value)}})
#define NULL_VAL        ((Value) {VAL_NULL, {.number = 0}})
#define NUM_VAL(value)  ((Value) {VAL_NUM, {.number = (value)}})
#define OBJ_VAL(object) ((Value) {VAL_OBJ, {.obj = (Obj *) (object)}})
#define ERR_VAL         ((Value) {VAL_ERR, {.number = 0}})

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
bool valuesEqual(Value a, Value b);
ObjString *valueToString(Value *value);

#endif // YAPL_VALUE_H
