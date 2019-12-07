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
#define FALCON_IS_BOOL(value) ((value).type == VAL_BOOL)
#define FALCON_IS_NULL(value) ((value).type == VAL_NULL)
#define FALCON_IS_NUM(value)  ((value).type == VAL_NUM)
#define FALCON_IS_OBJ(value)  ((value).type == VAL_OBJ)
#define FALCON_IS_ERR(value)  ((value).type == VAL_ERR)

/* Gets the C value from a Falcon Value */
#define FALCON_AS_BOOL(value) ((value).as.boolean)
#define FALCON_AS_NUM(value)  ((value).as.number)
#define FALCON_AS_OBJ(value)  ((value).as.obj)

/* Sets a native C value to a Falcon Value */
#define FALCON_BOOL_VAL(value) ((FalconValue) {VAL_BOOL, {.boolean = (value)}})
#define FALCON_NUM_VAL(value)  ((FalconValue) {VAL_NUM, {.number = (value)}})
#define FALCON_OBJ_VAL(object) ((FalconValue) {VAL_OBJ, {.obj = (FalconObj *) (object)}})
#define FALCON_NULL_VAL        ((FalconValue) {VAL_NULL, {.number = 0}})
#define FALCON_ERR_VAL         ((FalconValue) {VAL_ERR, {.number = 0}})

/* Array of Values */
typedef struct {
    int count;
    int capacity;
    FalconValue *values;
} ValueArray;

/* ValueArray operations */
void falconInitValArray(ValueArray *valueArray);
void falconFreeValArray(FalconVM *vm, ValueArray *valueArray);
void falconWriteValArray(FalconVM *vm, ValueArray *valueArray, FalconValue value);

/* Value operations */
bool falconValEqual(FalconValue a, FalconValue b);
bool falconIsFalsey(FalconValue value);
char *falconValToString(FalconVM *vm, FalconValue *value);
void falconPrintVal(FalconValue value);

#endif // FALCON_VALUE_H
