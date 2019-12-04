/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * falcon_value.c: Falcon's value representation
 * See Falcon's license in the LICENSE file
 */

#include "falcon_value.h"
#include "falcon_memory.h"
#include "falcon_object.h"
#include <stdio.h>
#include <string.h>

/**
 * Initializes an empty ValueArray.
 */
void FalconInitValues(FalconValueArray *valueArray) {
    valueArray->count = 0;
    valueArray->capacity = 0;
    valueArray->values = NULL;
}

/**
 * Frees a ValueArray.
 */
void FalconFreeValues(FalconVM *vm, FalconValueArray *valueArray) {
    FALCON_FREE_ARRAY(vm, FalconValue, valueArray->values, valueArray->capacity);
    FalconInitValues(valueArray);
}

/**
 * Appends a Value to the end of a ValueArray. If the current size is not enough, the capacity of
 * the array is increased to fit the new Value.
 */
void FalconWriteValues(FalconVM *vm, FalconValueArray *valueArray, FalconValue value) {
    if (valueArray->capacity < valueArray->count + 1) {
        int oldCapacity = valueArray->capacity;
        valueArray->capacity = FALCON_INCREASE_CAPACITY(oldCapacity);
        valueArray->values = FALCON_INCREASE_ARRAY(vm, valueArray->values, FalconValue, oldCapacity,
                                                   valueArray->capacity);

        if (valueArray->values == NULL) { /* Checks if the allocation failed */
            FalconMemoryError();
            return;
        }
    }

    valueArray->values[valueArray->count] = value;
    valueArray->count++;
}

/**
 * Marks a Falcon Value for garbage collection.
 */
void FalconMarkValue(FalconValue value) {
    if (!FALCON_IS_OBJ(value)) return; /* Num, bool, and null are not dynamically allocated */
    FalconMarkObject(FALCON_AS_OBJ(value));
}

/**
 * Checks if two Falcon Values are equal.
 */
bool FalconValuesEqual(FalconValue a, FalconValue b) {
    if (a.type != b.type) return false;

    switch (a.type) {
        case FALCON_VAL_BOOL:
            return FALCON_AS_BOOL(a) == FALCON_AS_BOOL(b);
        case FALCON_VAL_NULL:
            return true;
        case FALCON_VAL_NUM:
            return FALCON_AS_NUM(a) == FALCON_AS_NUM(b);
        case FALCON_VAL_OBJ:
            return FALCON_AS_OBJ(a) == FALCON_AS_OBJ(b);
        default:
            return false;
    }
}

/**
 * Takes the logical not (falsiness) of a value. In Falcon, 'null', 'false', the number zero, and an
 * empty string are falsey, while every other value behaves like 'true'.
 */
bool FalconIsFalsey(FalconValue value) {
    return FALCON_IS_NULL(value) || (FALCON_IS_BOOL(value) && !FALCON_AS_BOOL(value)) ||
           (FALCON_IS_NUM(value) && FALCON_AS_NUM(value) == 0) ||
           (FALCON_IS_STRING(value) && FALCON_AS_STRING(value)->length == 0);
}

/* String conversion constants */
#define FALCON_MAX_NUM_TO_STR       24
#define FALCON_NUM_TO_STR_FORMATTER "%.14g"

/**
 * Converts a given Falcon Value to a Falcon string.
 */
char *FalconValueToString(FalconVM *vm, FalconValue *value) {
    char *string = NULL;

    switch (value->type) {
        case FALCON_VAL_BOOL:
            string = (FALCON_AS_BOOL(*value) ? "true" : "false");
            break;
        case FALCON_VAL_NULL:
            string = "null";
            break;
        case FALCON_VAL_NUM:
            string = FALCON_ALLOCATE(vm, char, FALCON_MAX_NUM_TO_STR);
            sprintf(string, FALCON_NUM_TO_STR_FORMATTER, FALCON_AS_NUM(*value));
            break;
        case FALCON_VAL_OBJ:
            switch (FALCON_OBJ_TYPE(*value)) {
                case FALCON_OBJ_CLOSURE: {
                    FalconObjClosure *closure = FALCON_AS_CLOSURE(*value);
                    string = FALCON_ALLOCATE(vm, char, closure->function->name->length + 6);
                    sprintf(string, "<fn %s>", closure->function->name->chars);
                    break;
                }
                case FALCON_OBJ_FUNCTION: {
                    FalconObjFunction *function = FALCON_AS_FUNCTION(*value);
                    string = FALCON_ALLOCATE(vm, char, function->name->length + 6);
                    sprintf(string, "<fn %s>", function->name->chars);
                    break;
                }
                case FALCON_OBJ_NATIVE: {
                    FalconObjNative *native = FALCON_AS_NATIVE(*value);
                    string = FALCON_ALLOCATE(vm, char, strlen(native->functionName) + 13);
                    sprintf(string, "<native fn %s>", native->functionName);
                    break;
                }
                default:
                    break;
            }
        default:
            break;
    }

    return string; /* Returns the string */
}

#undef FALCON_MAX_NUM_TO_STR
#undef FALCON_NUM_TO_STR_FORMATTER

/**
 * Prints a single Falcon Value.
 */
void FalconPrintValue(FalconValue value) {
    switch (value.type) {
        case FALCON_VAL_BOOL:
            printf(FALCON_AS_BOOL(value) ? "true" : "false");
            break;
        case FALCON_VAL_NULL:
            printf("null");
            break;
        case FALCON_VAL_NUM:
            printf("%g", FALCON_AS_NUM(value));
            break;
        case FALCON_VAL_OBJ:
            FalconPrintObject(value);
            break;
        default:
            break;
    }
}
