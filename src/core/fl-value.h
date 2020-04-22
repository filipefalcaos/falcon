/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * fl-value.h: Falcon's value representation
 * See Falcon's license in the LICENSE file
 */

#ifndef FL_VALUE_H
#define FL_VALUE_H

#include "../falcon.h"
#include <stdbool.h>

/* Checks the type of a FalconValue. It returns a boolean value, indicating whether the value is of
 * the macro type */
#define IS_BOOL(value) ((value).type == VAL_BOOL)
#define IS_NULL(value) ((value).type == VAL_NULL)
#define IS_NUM(value)  ((value).type == VAL_NUM)
#define IS_OBJ(value)  ((value).type == VAL_OBJ)
#define IS_ERR(value)  ((value).type == VAL_ERR)

/* Casts a FalconValue to a specific value type. No validation is performed, so C errors will
 * likely rise if the value type was not tested previously (see the macros above) */
#define AS_BOOL(value) ((value).as.boolean)
#define AS_NUM(value)  ((value).as.number)
#define AS_OBJ(value)  ((value).as.obj)

/* Makes a FalconValue from a primitive C value */
#define BOOL_VAL(value) ((FalconValue){VAL_BOOL, {.boolean = (value)}})
#define NUM_VAL(value)  ((FalconValue){VAL_NUM, {.number = (value)}})
#define OBJ_VAL(object) ((FalconValue){VAL_OBJ, {.obj = (FalconObj *) (object)}})
#define NULL_VAL        ((FalconValue){VAL_NULL, {.number = 0}})
#define ERR_VAL         ((FalconValue){VAL_ERR, {.number = 0}})

/* String conversion constants */
#define MIN_COLLECTION_TO_STR 10
#define MAX_NUM_TO_STR        24
#define NUM_TO_STR_FORMATTER  "%.14g"

/* Forward object declarations */
typedef struct _Obj FalconObj;
typedef struct _ObjString ObjString;

/* Types of values on Falcon */
typedef enum {
    VAL_BOOL, /* "true" or "false" */
    VAL_NULL, /* "null" */
    VAL_NUM,  /* IEEE double precision number */
    VAL_OBJ,  /* Heap allocated objects (see "falcon_object.h") */
    VAL_ERR   /* Special value that marks an error in a native function */
} ValueType;

/* Defines the built-in types representation. Booleans, numbers, and null are unboxed (see the C
 * union "as" below). Classes and their instances, functions, lists, maps, and strings are
 * heap-allocated objects. The FalconValue union stores only a pointer to these objects */
typedef struct {
    ValueType type;
    union {
        bool boolean;
        double number;
        FalconObj *obj;
    } as;
} FalconValue;

/* A dynamic array of FalconValues that grows as much as needed */
typedef struct {
    int count;
    int capacity;
    FalconValue *values;
} ValueArray;

/* Initializes an empty dynamic array of FalconValues */
void init_value_array(ValueArray *valueArray);

/* Frees a dynamic array of FalconValues */
void free_value_array(FalconVM *vm, ValueArray *valueArray);

/* Appends a given FalconValue to the end of a ValueArray */
void write_value_array(FalconVM *vm, ValueArray *valueArray, FalconValue value);

/* Checks if two FalconValues are equal. For unboxed values, this is a value comparison, while for
 * object values, this is an identity comparison */
bool values_equal(FalconValue a, FalconValue b);

/* Takes the "logical not" of a FalconValue. In Falcon, "null", "false", the number zero, and an
 * empty string are falsy, while every other value behaves like "true". */
bool is_falsy(FalconValue value);

/* Converts a given FalconValue, that is not already a string, into a ObjString */
ObjString *value_to_string(FalconVM *vm, FalconValue *value);

/* Prints a single FalconValue to stdout */
void print_value(FalconVM *vm, FalconValue value);

#endif // FL_VALUE_H
