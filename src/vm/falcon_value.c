/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * falcon_value.c: Falcon's value representation
 * See Falcon's license in the LICENSE file
 */

#include "falcon_value.h"
#include "falcon_memory.h"
#include <stdio.h>
#include <string.h>

/**
 * Initializes an empty ValueArray.
 */
void falconInitValArray(ValueArray *valueArray) {
    valueArray->count = 0;
    valueArray->capacity = 0;
    valueArray->values = NULL;
}

/**
 * Frees a ValueArray.
 */
void falconFreeValArray(FalconVM *vm, ValueArray *valueArray) {
    FALCON_FREE_ARRAY(vm, FalconValue, valueArray->values, valueArray->capacity);
    falconInitValArray(valueArray);
}

/**
 * Appends a Value to the end of a ValueArray. If the current size is not enough, the capacity of
 * the array is increased to fit the new Value.
 */
void falconWriteValArray(FalconVM *vm, ValueArray *valueArray, FalconValue value) {
    if (valueArray->capacity < valueArray->count + 1) {
        int oldCapacity = valueArray->capacity;
        valueArray->capacity = FALCON_INCREASE_CAPACITY(oldCapacity);
        valueArray->values = FALCON_INCREASE_ARRAY(vm, valueArray->values, FalconValue, oldCapacity,
                                                   valueArray->capacity);
    }

    valueArray->values[valueArray->count] = value;
    valueArray->count++;
}

/**
 * Checks if two Falcon Values are equal.
 */
bool falconValEqual(FalconValue a, FalconValue b) {
    if (a.type != b.type) return false;

    switch (a.type) {
        case VAL_BOOL:
            return FALCON_AS_BOOL(a) == FALCON_AS_BOOL(b);
        case VAL_NULL:
            return true;
        case VAL_NUM:
            return FALCON_AS_NUM(a) == FALCON_AS_NUM(b);
        case VAL_OBJ:
            return FALCON_AS_OBJ(a) == FALCON_AS_OBJ(b);
        default:
            return false;
    }
}

/**
 * Takes the logical not of a value. In Falcon, "null", "false", the number zero, and an empty
 * string are falsy, while every other value behaves like "true".
 */
bool falconIsFalsy(FalconValue value) {
    return FALCON_IS_NULL(value) || (FALCON_IS_BOOL(value) && !FALCON_AS_BOOL(value)) ||
           (FALCON_IS_NUM(value) && FALCON_AS_NUM(value) == 0) ||
           (FALCON_IS_STRING(value) && FALCON_AS_STRING(value)->length == 0) ||
           (FALCON_IS_LIST(value) && FALCON_AS_LIST(value)->elements.count == 0);
}

/* String conversion constants */
#define MIN_LIST_TO_STR      3
#define MAX_NUM_TO_STR       24
#define NUM_TO_STR_FORMATTER "%.14g"

/**
 * Converts a given Falcon Function to a Falcon string.
 */
char *functionToString(FalconVM *vm, ObjFunction *function) {
    if (function->name == NULL) {
        return FALCON_SCRIPT;
    } else {
        char *string = FALCON_ALLOCATE(vm, char, function->name->length + 6);
        sprintf(string, "<fn %s>", function->name->chars);
        return string;
    }
}

/**
 * Converts a given Falcon List to a Falcon string.
 */
char *listToString(FalconVM *vm, ObjList *list) {
    int allocationSize = MIN_LIST_TO_STR;
    char *string = FALCON_ALLOCATE(vm, char, allocationSize); /* Initial allocation */
    int stringLen = snprintf(string, allocationSize, "[");

    for (int i = 0; i < list->elements.count; i++) {
        bool isString;
        char *elementString;
        size_t elementLen;

        /* Gets the current element string */
        if (FALCON_IS_STRING(list->elements.values[i])) {
            ObjString *objString = FALCON_AS_STRING(list->elements.values[i]);
            elementString = objString->chars;
            elementLen = objString->length;
            isString = true;
        } else {
            elementString = falconValToString(vm, &list->elements.values[i]);
            elementLen = strlen(elementString);
            isString = false;
        }

        /* Increases the allocation, if needed */
        if ((stringLen + elementLen + 2) > allocationSize) { /* +2 for a possible separator */
            int oldSize = allocationSize;
            allocationSize = FALCON_INCREASE_CAPACITY(allocationSize);
            string = FALCON_INCREASE_ARRAY(vm, string, char, oldSize, allocationSize);
        }

        /* Appends the current element to the output string */
        stringLen += snprintf(string + stringLen, allocationSize - stringLen, "%s", elementString);
        if (!isString) FALCON_FREE(vm, char, elementString);

        if (i != list->elements.count - 1) /* Should print the separator after element? */
            stringLen += snprintf(string + stringLen, allocationSize - stringLen, ", ");
    }

    snprintf(string + stringLen, allocationSize - stringLen, "]");
    return string;
}

/**
 * Converts a given Falcon Value that is not already a string into a Falcon string.
 */
char *falconValToString(FalconVM *vm, FalconValue *value) {
    switch (value->type) {
        case VAL_BOOL: {
            char *string = FALCON_ALLOCATE(vm, char, 6);
            sprintf(string, "%s", FALCON_AS_BOOL(*value) ? "true" : "false");
            return string;
        }
        case VAL_NULL: {
            char *string = FALCON_ALLOCATE(vm, char, 5);
            sprintf(string, "%s", "null");
            return string;
        }
        case VAL_NUM: {
            char *string = FALCON_ALLOCATE(vm, char, MAX_NUM_TO_STR);
            sprintf(string, NUM_TO_STR_FORMATTER, FALCON_AS_NUM(*value));
            return string;
        }
        case VAL_OBJ:
            switch (FALCON_OBJ_TYPE(*value)) {
                case OBJ_CLOSURE: {
                    ObjClosure *closure = FALCON_AS_CLOSURE(*value);
                    return functionToString(vm, closure->function);
                }
                case OBJ_FUNCTION: {
                    ObjFunction *function = FALCON_AS_FUNCTION(*value);
                    return functionToString(vm, function);
                }
                case OBJ_NATIVE: {
                    ObjNative *native = FALCON_AS_NATIVE(*value);
                    char *string = FALCON_ALLOCATE(vm, char, strlen(native->name) + 13);
                    sprintf(string, "<native fn %s>", native->name);
                    return string;
                }
                case OBJ_LIST: {
                    ObjList *list = FALCON_AS_LIST(*value);
                    return listToString(vm, list);
                }
                default:
                    break;
            }
        default:
            return NULL;
    }
}

#undef MAX_NUM_TO_STR
#undef NUM_TO_STR_FORMATTER

/**
 * Prints a given Falcon Function.
 */
void printFunction(ObjFunction *function) {
    if (function->name == NULL) {
        printf("%s", FALCON_SCRIPT);
    } else {
        printf("<fn %s>", function->name->chars);
    }
}

/**
 * Prints a single Falcon Value.
 */
void falconPrintVal(FalconVM *vm, FalconValue value, bool printQuotes) {
    switch (value.type) {
        case VAL_BOOL:
            printf("%s", FALCON_AS_BOOL(value) ? "true" : "false");
            break;
        case VAL_NULL:
            printf("null");
            break;
        case VAL_NUM:
            printf("%g", FALCON_AS_NUM(value));
            break;
        case VAL_OBJ:
            switch (FALCON_OBJ_TYPE(value)) {
                case OBJ_CLOSURE: {
                    ObjClosure *closure = FALCON_AS_CLOSURE(value);
                    printFunction(closure->function);
                    break;
                }
                case OBJ_FUNCTION: {
                    ObjFunction *function = FALCON_AS_FUNCTION(value);
                    printFunction(function);
                    break;
                }
                case OBJ_NATIVE: {
                    ObjNative *native = FALCON_AS_NATIVE(value);
                    printf("<native fn %s>", native->name);
                    break;
                }
                case OBJ_STRING: {
                    if (printQuotes) printf("\"");
                    printf("%s", FALCON_AS_CSTRING(value));
                    if (printQuotes) printf("\"");
                    break;
                }
                case OBJ_LIST: {
                    ObjList *list = FALCON_AS_LIST(value);
                    printf("[");

                    for (int i = 0; i < list->elements.count; i++) {
                        falconPrintVal(vm, list->elements.values[i], printQuotes);
                        if (i != list->elements.count - 1) printf(", ");
                    }

                    printf("]");
                    break;
                }
                default:
                    break;
            }
        default:
            break;
    }
}
