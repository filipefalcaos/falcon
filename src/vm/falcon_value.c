/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * falcon_value.c: Falcon's value representation
 * See Falcon's license in the LICENSE file
 */

#include "falcon_value.h"
#include "../lib/falcon_list.h"
#include "../lib/falcon_map.h"
#include "falcon_memory.h"
#include <stdio.h>
#include <string.h>

/**
 * Initializes an empty ValueArray.
 */
void initValArray(ValueArray *valueArray) {
    valueArray->count = 0;
    valueArray->capacity = 0;
    valueArray->values = NULL;
}

/**
 * Frees a ValueArray.
 */
void freeValArray(FalconVM *vm, ValueArray *valueArray) {
    FALCON_FREE_ARRAY(vm, FalconValue, valueArray->values, valueArray->capacity);
    initValArray(valueArray);
}

/**
 * Appends a Value to the end of a ValueArray. If the current size is not enough, the capacity of
 * the array is increased to fit the new Value.
 */
void writeValArray(FalconVM *vm, ValueArray *valueArray, FalconValue value) {
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
bool valuesEqual(FalconValue a, FalconValue b) {
    if (a.type != b.type) return false;

    switch (a.type) {
        case VAL_BOOL: return AS_BOOL(a) == AS_BOOL(b);
        case VAL_NULL: return true;
        case VAL_NUM: return AS_NUM(a) == AS_NUM(b);
        case VAL_OBJ: return AS_OBJ(a) == AS_OBJ(b);
        default: return false;
    }
}

/**
 * Takes the logical not of a value. In Falcon, "null", "false", the number zero, and an empty
 * string are falsy, while every other value behaves like "true".
 */
bool isFalsy(FalconValue value) {
    return IS_NULL(value) || (IS_BOOL(value) && !AS_BOOL(value)) ||
           (IS_NUM(value) && AS_NUM(value) == 0) ||
           (IS_STRING(value) && AS_STRING(value)->length == 0) ||
           (IS_LIST(value) && AS_LIST(value)->elements.count == 0) ||
           (IS_MAP(value) && AS_MAP(value)->entries.count == 0);
}

/**
 * Converts a given Falcon Function to a Falcon string.
 */
ObjString *functionToString(FalconVM *vm, ObjFunction *function) {
    if (function->name == NULL) {
        return falconString(vm, FALCON_SCRIPT, 6);
    } else {
        char *string = FALCON_ALLOCATE(vm, char, function->name->length + 6);
        sprintf(string, "<fn %s>", function->name->chars);
        return falconString(vm, string, strlen(string));
    }
}

/**
 * Converts a given Falcon Value that is not already a string into a Falcon string.
 */
ObjString *valueToString(FalconVM *vm, FalconValue *value) {
    switch (value->type) {
        case VAL_BOOL: {
            char *string = FALCON_ALLOCATE(vm, char, 6);
            sprintf(string, "%s", AS_BOOL(*value) ? "true" : "false");
            return falconString(vm, string, strlen(string));
        }
        case VAL_NULL: {
            char *string = FALCON_ALLOCATE(vm, char, 5);
            sprintf(string, "%s", "null");
            return falconString(vm, string, strlen(string));
        }
        case VAL_NUM: {
            char *string = FALCON_ALLOCATE(vm, char, MAX_NUM_TO_STR);
            sprintf(string, NUM_TO_STR_FORMATTER, AS_NUM(*value));
            return falconString(vm, string, strlen(string));
        }
        case VAL_OBJ:
            switch (OBJ_TYPE(*value)) {
                case OBJ_FUNCTION: {
                    ObjFunction *function = AS_FUNCTION(*value);
                    return functionToString(vm, function);
                }
                case OBJ_CLOSURE: {
                    ObjClosure *closure = AS_CLOSURE(*value);
                    return functionToString(vm, closure->function);
                }
                case OBJ_CLASS: {
                    ObjClass *class_ = AS_CLASS(*value);
                    char *string = FALCON_ALLOCATE(vm, char, class_->name->length + 9);
                    sprintf(string, "<class %s>", class_->name->chars);
                    return falconString(vm, string, strlen(string));
                }
                case OBJ_INSTANCE: {
                    ObjInstance *instance = AS_INSTANCE(*value);
                    char *string = FALCON_ALLOCATE(vm, char, instance->class_->name->length + 15);
                    sprintf(string, "<instance of %s>", instance->class_->name->chars);
                    return falconString(vm, string, strlen(string));
                }
                case OBJ_NATIVE: {
                    ObjNative *native = AS_NATIVE(*value);
                    char *string = FALCON_ALLOCATE(vm, char, strlen(native->name) + 13);
                    sprintf(string, "<native fn %s>", native->name);
                    return falconString(vm, string, strlen(string));
                }
                case OBJ_BMETHOD: {
                    ObjBMethod *bMethod = AS_BMETHOD(*value);
                    ObjString *methodName = bMethod->method->function->name;
                    char *string = FALCON_ALLOCATE(vm, char, methodName->length + 10);
                    sprintf(string, "<method %s>", methodName->chars);
                    return falconString(vm, string, strlen(string));
                }
                case OBJ_LIST: return listToString(vm, AS_LIST(*value));
                case OBJ_MAP: return mapToString(vm, AS_MAP(*value));
                default: break;
            }
        default: return NULL;
    }
}

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
void printValue(FalconVM *vm, FalconValue value) {
    switch (value.type) {
        case VAL_BOOL: printf("%s", AS_BOOL(value) ? "true" : "false"); break;
        case VAL_NULL: printf("null"); break;
        case VAL_NUM: printf("%g", AS_NUM(value)); break;
        case VAL_OBJ:
            switch (OBJ_TYPE(value)) {
                case OBJ_STRING: printf("\"%s\"", AS_CSTRING(value)); break;
                case OBJ_FUNCTION: printFunction(AS_FUNCTION(value)); break;
                case OBJ_CLOSURE: printFunction(AS_CLOSURE(value)->function); break;
                case OBJ_CLASS: printf("<class %s>", AS_CLASS(value)->name->chars); break;
                case OBJ_BMETHOD: printFunction(AS_BMETHOD(value)->method->function); break;
                case OBJ_LIST: printf("%s", listToString(vm, AS_LIST(value))->chars); break;
                case OBJ_MAP: printf("%s", mapToString(vm, AS_MAP(value))->chars); break;
                case OBJ_NATIVE: printf("<native fn %s>", AS_NATIVE(value)->name); break;
                case OBJ_INSTANCE:
                    printf("<instance of %s>", AS_INSTANCE(value)->class_->name->chars);
                    break;
                default: break;
            }
        default: break;
    }
}
