/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * fl-baselib.c: Falcon's basic library
 * See Falcon's license in the LICENSE file
 */

#include "fl-baselib.h"
#include "../core/fl-mem.h"
#include <stdio.h>
#include <stdlib.h>

/**
 * Prints to stdout (with a new line at the end) a given list of FalconValues. Any type of value in
 * Falcon can be printed and any number of arguments are accepted.
 */
FalconValue lib_print(FalconVM *vm, int argCount, FalconValue *args) {
    if (argCount > 1) {
        for (int i = 0; i < argCount; i++) {
            print_value(vm, args[i]);
            if (i < argCount - 1) printf(" "); /* Separator */
        }
    } else {
        print_value(vm, *args);
    }

    printf("\n"); /* End */
    return NULL_VAL;
}

/**
 * Returns the type of a given FalconValue, as a ObjString. It takes only one argument, of any
 * Falcon type.
 */
FalconValue lib_type(FalconVM *vm, int argCount, FalconValue *args) {
    ASSERT_ARGS_COUNT(vm, !=, argCount, 1);

    /* Checks the value type */
    switch (args->type) {
        case VAL_BOOL: return OBJ_VAL(new_ObjString(vm, "bool", 4));
        case VAL_NULL: return OBJ_VAL(new_ObjString(vm, "null", 4));
        case VAL_NUM: return OBJ_VAL(new_ObjString(vm, "number", 6));
        case VAL_OBJ:
            switch (OBJ_TYPE(*args)) {
                case OBJ_STRING: return OBJ_VAL(new_ObjString(vm, "string", 6));
                case OBJ_CLASS: return OBJ_VAL(new_ObjString(vm, "class", 5));
                case OBJ_LIST: return OBJ_VAL(new_ObjString(vm, "list", 4));
                case OBJ_MAP: return OBJ_VAL(new_ObjString(vm, "map", 3));
                case OBJ_BMETHOD: return OBJ_VAL(new_ObjString(vm, "method", 6));
                case OBJ_CLOSURE:
                case OBJ_FUNCTION:
                case OBJ_NATIVE: return OBJ_VAL(new_ObjString(vm, "function", 8));
                case OBJ_INSTANCE: {
                    ObjInstance *instance = AS_INSTANCE(*args);
                    char *typeStr = FALCON_ALLOCATE(vm, char, instance->class_->name->length + 6);
                    sprintf(typeStr, "class %s", instance->class_->name->chars);
                    return OBJ_VAL(new_ObjString(vm, typeStr, instance->class_->name->length + 6));
                }
                default: break;
            }
            break;
        default: break;
    }

    return OBJ_VAL(new_ObjString(vm, "unknown", 7)); /* Unknown type */
}

/**
 * Converts a given FalconValue to a boolean. It takes only one argument, of any Falcon type. The
 * conversion is implemented through the "is_falsy" function.
 */
FalconValue lib_bool(FalconVM *vm, int argCount, FalconValue *args) {
    ASSERT_ARGS_COUNT(vm, !=, argCount, 1);
    if (!IS_BOOL(*args)) return BOOL_VAL(!is_falsy(*args));
    return *args; /* Given value is already a boolean */
}

/**
 * Converts a given FalconValue to a number. It takes only one argument that must be either a
 * string, a boolean, or numeric. If the conversion cannot be done, a error will be reported.
 */
FalconValue lib_num(FalconVM *vm, int argCount, FalconValue *args) {
    ASSERT_ARGS_COUNT(vm, !=, argCount, 1);
    if (!IS_NUM(*args)) {
        if (!IS_STRING(*args) && !IS_BOOL(*args)) {
            interpreter_error(vm, VM_ARGS_TYPE_ERR, 1, "string, boolean, or number");
            return ERR_VAL;
        }

        if (IS_STRING(*args)) { /* String to number */
            char *start = AS_CSTRING(*args), *end;
            double number = strtod(AS_CSTRING(*args), &end); /* Converts to double */

            if (start == end) { /* Checks for conversion success */
                interpreter_error(vm, FALCON_CONV_STR_NUM_ERR);
                return ERR_VAL;
            }

            return NUM_VAL(number);
        } else { /* Boolean to number */
            return NUM_VAL(AS_BOOL(*args) ? 1 : 0);
        }
    }

    return *args; /* Given value is already a number */
}

/**
 * Converts a given FalconValue to a ObjString. It takes only one argument, of any Falcon type. The
 * conversion is implemented through the "value_to_string" function.
 */
FalconValue lib_str(FalconVM *vm, int argCount, FalconValue *args) {
    ASSERT_ARGS_COUNT(vm, !=, argCount, 1);
    return OBJ_VAL(value_to_string(vm, args)); /* Converts value to a string */
}

/**
 * Returns the length of a given FalconValue. It takes only one argument that must be either a
 * string, a list, or a map. If the computing cannot be done, a error will be reported.
 */
FalconValue lib_len(FalconVM *vm, int argCount, FalconValue *args) {
    ASSERT_ARGS_COUNT(vm, !=, argCount, 1);
    ASSERT_ARG_TYPE(IS_OBJ, "list, map or string", *args, vm, 1);

    /* Handles the subscript types */
    switch (AS_OBJ(*args)->type) {
        case OBJ_LIST: return NUM_VAL(AS_LIST(*args)->elements.count); /* Returns the list length */
        case OBJ_MAP: return NUM_VAL(AS_MAP(*args)->count);            /* Returns the map length */
        case OBJ_STRING: return NUM_VAL(AS_STRING(*args)->length); /* Returns the string length */
        default: interpreter_error(vm, VM_ARGS_TYPE_ERR, 1, "list, map, or string"); return ERR_VAL;
    }
}

/**
 * Returns whether a given FalconValue has a given field. It takes two arguments: a class instance
 * and a ObjString (the field).
 */
FalconValue lib_hasField(FalconVM *vm, int argCount, FalconValue *args) {
    ASSERT_ARGS_COUNT(vm, !=, argCount, 2);
    ASSERT_ARG_TYPE(IS_INSTANCE, "class instance", args[0], vm, 1);
    ASSERT_ARG_TYPE(IS_STRING, "string", args[1], vm, 2);

    /* Checks if the field is defined */
    ObjInstance *instance = AS_INSTANCE(args[0]);
    FalconValue value;
    return BOOL_VAL(map_get(&instance->fields, AS_STRING(args[1]), &value));
}

/**
 * Returns the value of a given field from a given FalconValue. It takes two arguments: a class
 * instance and a ObjString (the field). If the field does not exist in the instance, an error will
 * be reported.
 */
FalconValue lib_getField(FalconVM *vm, int argCount, FalconValue *args) {
    ASSERT_ARGS_COUNT(vm, !=, argCount, 2);
    ASSERT_ARG_TYPE(IS_INSTANCE, "class instance", args[0], vm, 1);
    ASSERT_ARG_TYPE(IS_STRING, "string", args[1], vm, 2);

    /* Gets the field value */
    ObjInstance *instance = AS_INSTANCE(args[0]);
    FalconValue value;
    if (map_get(&instance->fields, AS_STRING(args[1]), &value)) return value;

    /* Undefined field error */
    interpreter_error(vm, VM_UNDEF_PROP_ERR, instance->class_->name->chars,
                      AS_STRING(args[1])->chars);
    return ERR_VAL;
}

/**
 * Sets a given FalconValue to a given field from another given FalconValue. It takes three
 * arguments: a class instance, a ObjString (the field), and a value to assign.
 */
FalconValue lib_setField(FalconVM *vm, int argCount, FalconValue *args) {
    ASSERT_ARGS_COUNT(vm, !=, argCount, 3);
    ASSERT_ARG_TYPE(IS_INSTANCE, "class instance", args[0], vm, 1);
    ASSERT_ARG_TYPE(IS_STRING, "string", args[1], vm, 2);

    /* Sets the field value */
    ObjInstance *instance = AS_INSTANCE(args[0]);
    map_set(vm, &instance->fields, AS_STRING(args[1]), args[2]);
    return args[2];
}

/**
 * Deletes a given field from a given FalconValue. It takes two arguments: a class instance and a
 * ObjString (the field). Null is always returned.
 */
FalconValue lib_delField(FalconVM *vm, int argCount, FalconValue *args) {
    ASSERT_ARGS_COUNT(vm, !=, argCount, 2);
    ASSERT_ARG_TYPE(IS_INSTANCE, "class instance", args[0], vm, 1);
    ASSERT_ARG_TYPE(IS_STRING, "string", args[1], vm, 2);

    /* Deletes the field */
    ObjInstance *instance = AS_INSTANCE(args[0]);
    map_remove(&instance->fields, AS_STRING(args[1]));
    return NULL_VAL;
}
