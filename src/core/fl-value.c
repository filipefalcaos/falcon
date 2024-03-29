/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * fl-value.c: Falcon's value representation
 * See Falcon's license in the LICENSE file
 */

#include "fl-value.h"
#include "../lib/fl-listlib.h"
#include "../lib/fl-maplib.h"
#include "fl-mem.h"
#include <stdio.h>
#include <string.h>

/**
 * Initializes an empty dynamic array of FalconValues.
 */
void init_value_array(ValueArray *valueArray) {
    valueArray->count = 0;
    valueArray->capacity = 0;
    valueArray->values = NULL;
}

/**
 * Frees a dynamic array of FalconValues.
 */
void free_value_array(FalconVM *vm, ValueArray *valueArray) {
    FALCON_FREE_ARRAY(vm, FalconValue, valueArray->values, valueArray->capacity);
    init_value_array(valueArray);
}

/**
 * Appends a given FalconValue to the end of a ValueArray. If the current size is not enough, the
 * capacity of the array is increased to fit the new Value.
 */
void write_value_array(FalconVM *vm, ValueArray *valueArray, FalconValue value) {
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
 * Checks if two FalconValues are equal. For unboxed values, this is a value comparison, while for
 * object values, this is an identity comparison.
 */
bool values_equal(FalconValue a, FalconValue b) {
    if (a.type != b.type) return false;

    switch (a.type) {
        case VAL_BOOL:
            return AS_BOOL(a) == AS_BOOL(b);
        case VAL_NULL:
            return true;
        case VAL_NUM:
            return AS_NUM(a) == AS_NUM(b);
        case VAL_OBJ:
            return AS_OBJ(a) == AS_OBJ(b);
        default:
            return false;
    }
}

/**
 * Takes the "logical not" of a FalconValue. In Falcon, "null", "false", the number zero, and an
 * empty string are falsy, while every other value behaves like "true".
 */
bool is_falsy(FalconValue value) {
    return IS_NULL(value) || (IS_BOOL(value) && !AS_BOOL(value)) ||
           (IS_NUM(value) && AS_NUM(value) == 0) ||
           (IS_STRING(value) && AS_STRING(value)->length == 0) ||
           (IS_LIST(value) && AS_LIST(value)->elements.count == 0) ||
           (IS_MAP(value) && AS_MAP(value)->count == 0);
}

/**
 * Converts a given ObjFunction to a ObjString.
 */
ObjString *fn_to_string(FalconVM *vm, ObjFunction *function) {
    if (function->name == NULL) {
        return new_ObjString(vm, FALCON_SCRIPT, 6);
    } else {
        char *string = FALCON_ALLOCATE(vm, char, function->name->length + 6);
        sprintf(string, "<fn %s>", function->name->chars);
        return new_ObjString(vm, string, strlen(string));
    }
}

/**
 * Converts a given FalconValue, that is not already a string, into a ObjString.
 */
ObjString *value_to_string(FalconVM *vm, FalconValue *value) {
    if (IS_STRING(*value)) return AS_STRING(*value);

    switch (value->type) {
        case VAL_BOOL: {
            char *string = FALCON_ALLOCATE(vm, char, 6);
            sprintf(string, "%s", AS_BOOL(*value) ? "true" : "false");
            return new_ObjString(vm, string, strlen(string));
        }
        case VAL_NULL: {
            char *string = FALCON_ALLOCATE(vm, char, 5);
            sprintf(string, "%s", "null");
            return new_ObjString(vm, string, strlen(string));
        }
        case VAL_NUM: {
            char *string = FALCON_ALLOCATE(vm, char, MAX_NUM_TO_STR);
            sprintf(string, NUM_TO_STR_FORMATTER, AS_NUM(*value));
            return new_ObjString(vm, string, strlen(string));
        }
        case VAL_OBJ:
            switch (OBJ_TYPE(*value)) {
                case OBJ_FUNCTION: {
                    ObjFunction *function = AS_FUNCTION(*value);
                    return fn_to_string(vm, function);
                }
                case OBJ_CLOSURE: {
                    ObjClosure *closure = AS_CLOSURE(*value);
                    return fn_to_string(vm, closure->function);
                }
                case OBJ_CLASS: {
                    ObjClass *class_ = AS_CLASS(*value);
                    char *string = FALCON_ALLOCATE(vm, char, class_->name->length + 9);
                    sprintf(string, "<class %s>", class_->name->chars);
                    return new_ObjString(vm, string, strlen(string));
                }
                case OBJ_INSTANCE: {
                    ObjInstance *instance = AS_INSTANCE(*value);
                    char *string = FALCON_ALLOCATE(vm, char, instance->class_->name->length + 15);
                    sprintf(string, "<instance of %s>", instance->class_->name->chars);
                    return new_ObjString(vm, string, strlen(string));
                }
                case OBJ_NATIVE: {
                    ObjNative *native = AS_NATIVE(*value);
                    char *string = FALCON_ALLOCATE(vm, char, strlen(native->name) + 13);
                    sprintf(string, "<native fn %s>", native->name);
                    return new_ObjString(vm, string, strlen(string));
                }
                case OBJ_BMETHOD: {
                    ObjBMethod *bMethod = AS_BMETHOD(*value);
                    ObjString *methodName = bMethod->method->function->name;
                    char *string = FALCON_ALLOCATE(vm, char, methodName->length + 10);
                    sprintf(string, "<method %s>", methodName->chars);
                    return new_ObjString(vm, string, strlen(string));
                }
                case OBJ_LIST:
                    return list_to_string(vm, AS_LIST(*value));
                case OBJ_MAP:
                    return map_to_string(vm, AS_MAP(*value));
                default:
                    break;
            }
        default:
            return NULL;
    }
}

/**
 * Prints to stdout a given ObjFunction.
 */
void print_fn(ObjFunction *function) {
    if (function->name == NULL) {
        printf("%s", FALCON_SCRIPT);
    } else {
        printf("<fn %s>", function->name->chars);
    }
}

/**
 * Prints a single FalconValue to stdout.
 */
void print_value(FalconVM *vm, FalconValue value) {
    switch (value.type) {
        case VAL_BOOL:
            printf("%s", AS_BOOL(value) ? "true" : "false");
            break;
        case VAL_NULL:
            printf("null");
            break;
        case VAL_NUM:
            printf("%g", AS_NUM(value));
            break;
        case VAL_OBJ:
            switch (OBJ_TYPE(value)) {
                case OBJ_STRING:
                    printf("\"%s\"", AS_CSTRING(value));
                    break;
                case OBJ_FUNCTION:
                    print_fn(AS_FUNCTION(value));
                    break;
                case OBJ_CLOSURE:
                    print_fn(AS_CLOSURE(value)->function);
                    break;
                case OBJ_CLASS:
                    printf("<class %s>", AS_CLASS(value)->name->chars);
                    break;
                case OBJ_BMETHOD:
                    print_fn(AS_BMETHOD(value)->method->function);
                    break;
                case OBJ_LIST:
                    printf("%s", list_to_string(vm, AS_LIST(value))->chars);
                    break;
                case OBJ_MAP:
                    printf("%s", map_to_string(vm, AS_MAP(value))->chars);
                    break;
                case OBJ_NATIVE:
                    printf("<native fn %s>", AS_NATIVE(value)->name);
                    break;
                case OBJ_INSTANCE:
                    printf("<instance of %s>", AS_INSTANCE(value)->class_->name->chars);
                    break;
                default:
                    break;
            }
        default:
            break;
    }
}
