/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * falcon_value.h: Falcon's value representation
 * See Falcon's license in the LICENSE file
 */

#ifndef FALCON_VALUE_H
#define FALCON_VALUE_H

#include "../commons.h"

/* Forward object declaration */
typedef struct _Obj FalconObj;
typedef struct _ObjString ObjString;

/* Types of values on Falcon */
typedef enum { VAL_BOOL, VAL_NULL, VAL_NUM, VAL_OBJ, VAL_ERR } ValueType;

/* Value representation */
typedef struct {
    ValueType type;
    union {
        bool boolean;
        double number;
        FalconObj *obj;
    } as;
} FalconValue;

/* Checks a Value type */
#define IS_BOOL(value) ((value).type == VAL_BOOL)
#define IS_NULL(value) ((value).type == VAL_NULL)
#define IS_NUM(value)  ((value).type == VAL_NUM)
#define IS_OBJ(value)  ((value).type == VAL_OBJ)
#define IS_ERR(value)  ((value).type == VAL_ERR)

/* Gets the C value from a Falcon Value */
#define AS_BOOL(value) ((value).as.boolean)
#define AS_NUM(value)  ((value).as.number)
#define AS_OBJ(value)  ((value).as.obj)

/* Sets a native C value to a Falcon Value */
#define BOOL_VAL(value) ((FalconValue){VAL_BOOL, {.boolean = (value)}})
#define NUM_VAL(value)  ((FalconValue){VAL_NUM, {.number = (value)}})
#define OBJ_VAL(object) ((FalconValue){VAL_OBJ, {.obj = (FalconObj *) (object)}})
#define NULL_VAL        ((FalconValue){VAL_NULL, {.number = 0}})
#define ERR_VAL         ((FalconValue){VAL_ERR, {.number = 0}})

/* String conversion constants */
#define MIN_COLLECTION_TO_STR 10
#define MAX_NUM_TO_STR        24
#define NUM_TO_STR_FORMATTER  "%.14g"

/* Array of Values */
typedef struct {
    int count;
    int capacity;
    FalconValue *values;
} ValueArray;

/* ValueArray operations */
void initValArray(ValueArray *valueArray);
void freeValArray(FalconVM *vm, ValueArray *valueArray);
void writeValArray(FalconVM *vm, ValueArray *valueArray, FalconValue value);

/* Value operations */
bool valuesEqual(FalconValue a, FalconValue b);
bool isFalsy(FalconValue value);
ObjString *valueToString(FalconVM *vm, FalconValue *value);
void printValue(FalconVM *vm, FalconValue value);

#endif // FALCON_VALUE_H
