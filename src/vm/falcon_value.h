/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * falcon_value.h: Falcon's value representation
 * See Falcon's license in the LICENSE file
 */

#ifndef FALCON_VALUE_H
#define FALCON_VALUE_H

#include "../commons.h"

/* Forward object declaration */
typedef struct sObj FalconObj;
typedef struct sObjString FalconObjString;

/* Types of values on Falcon */
typedef enum {
    FALCON_VAL_BOOL,
    FALCON_VAL_NULL,
    FALCON_VAL_NUM,
    FALCON_VAL_OBJ,
    FALCON_VAL_ERR
} FalconValueType;

/* Value representation */
typedef struct {
    FalconValueType type;
    union {
        bool boolean;
        double number;
        FalconObj *obj;
    } as;
} FalconValue;

/* Checks a Value type */
#define FALCON_IS_BOOL(value) ((value).type == FALCON_VAL_BOOL)
#define FALCON_IS_NULL(value) ((value).type == FALCON_VAL_NULL)
#define FALCON_IS_NUM(value)  ((value).type == FALCON_VAL_NUM)
#define FALCON_IS_OBJ(value)  ((value).type == FALCON_VAL_OBJ)
#define FALCON_IS_ERR(value)  ((value).type == FALCON_VAL_ERR)

/* Gets the C value from a Falcon Value */
#define FALCON_AS_BOOL(value) ((value).as.boolean)
#define FALCON_AS_NUM(value)  ((value).as.number)
#define FALCON_AS_OBJ(value)  ((value).as.obj)

/* Sets a native C value to a Falcon Value */
#define FALCON_BOOL_VAL(value) ((FalconValue) {FALCON_VAL_BOOL, {.boolean = (value)}})
#define FALCON_NUM_VAL(value)  ((FalconValue) {FALCON_VAL_NUM, {.number = (value)}})
#define FALCON_OBJ_VAL(object) ((FalconValue) {FALCON_VAL_OBJ, {.obj = (FalconObj *) (object)}})
#define FALCON_NULL_VAL        ((FalconValue) {FALCON_VAL_NULL, {.number = 0}})
#define FALCON_ERR_VAL         ((FalconValue) {FALCON_VAL_ERR, {.number = 0}})

/* Array of Values */
typedef struct {
    int count;
    int capacity;
    FalconValue *values;
} FalconValueArray;

/* ValueArray operations */
void FalconInitValues(FalconValueArray *valueArray);
void FalconFreeValues(FalconValueArray *valueArray);
void FalconWriteValues(FalconValueArray *valueArray, FalconValue value);

/* Value operations */
void FalconPrintValue(FalconValue value);
bool FalconValuesEqual(FalconValue a, FalconValue b);
bool FalconIsFalsey(FalconValue value);
char *FalconValueToString(FalconValue *value);

/* Types of objects on Falcon */
typedef enum {
    FALCON_OBJ_STRING,
    FALCON_OBJ_UPVALUE,
    FALCON_OBJ_CLOSURE,
    FALCON_OBJ_FUNCTION,
    FALCON_OBJ_NATIVE
} FalconObjType;

/* Object representation */
struct sObj {
    FalconObjType type;
    struct sObj *next;
};

/* Falcon's string object */
struct sObjString {
    FalconObj obj;
    int length;
    uint32_t hash;
    char chars[]; /* Flexible array member */
};

/* Falcon's upvalue object */
typedef struct sUpvalue {
    FalconObj obj;
    FalconValue *slot;
    FalconValue closed;
    struct sUpvalue *next;
} FalconObjUpvalue;

/* Gets a object type from an Falcon Value */
#define FALCON_OBJ_TYPE(value) (FALCON_AS_OBJ(value)->type)

/* Checks if a Value is an FalconObj type */
#define FALCON_IS_STRING(value) isObjType(value, FALCON_OBJ_STRING)

/* Gets the object value from a Falcon Value */
#define FALCON_AS_CLOSURE(value)  ((FalconObjClosure *) FALCON_AS_OBJ(value))
#define FALCON_AS_FUNCTION(value) ((FalconObjFunction *) FALCON_AS_OBJ(value))
#define FALCON_AS_NATIVE(value)   (((FalconObjNative *) FALCON_AS_OBJ(value))->function)
#define FALCON_AS_STRING(value)   ((FalconObjString *) FALCON_AS_OBJ(value))
#define FALCON_AS_CSTRING(value)  (((FalconObjString *) FALCON_AS_OBJ(value))->chars)

/**
 * Checks if a Value is of an FalconObj type.
 */
static inline bool isObjType(FalconValue value, FalconObjType type) {
    return FALCON_IS_OBJ(value) && FALCON_AS_OBJ(value)->type == type;
}

#endif // FALCON_VALUE_H
