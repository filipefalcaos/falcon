/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * falcon_natives.c: Falcon's native functions
 * See Falcon's license in the LICENSE file
 */

#include "falcon_natives.h"
#include "../vm/falcon_memory.h"
#include "falcon_io.h"
#include "falcon_sys.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Defines a common interface to all Falcon native functions */
#define FALCON_NATIVE(name) static FalconValue name(FalconVM *vm, int argCount, FalconValue *args)

/*
 * ================================================================================================
 * ================================ System-related native functions ===============================
 * ================================================================================================
 */

/*
 * ================================================================================================
 * ================================= Type-related native functions ================================
 * ================================================================================================
 */

/**
 * Native Falcon function to get the type of a given Falcon Value, as a string.
 */
FALCON_NATIVE(type) {
    ASSERT_ARGS_COUNT(vm, !=, argCount, 1);

    /* Checks the value type */
    switch (args->type) {
        case VAL_BOOL: return OBJ_VAL(falconString(vm, "bool", 4));
        case VAL_NULL: return OBJ_VAL(falconString(vm, "null", 4));
        case VAL_NUM: return OBJ_VAL(falconString(vm, "number", 6));
        case VAL_OBJ:
            switch (OBJ_TYPE(*args)) {
                case OBJ_STRING: return OBJ_VAL(falconString(vm, "string", 6));
                case OBJ_CLASS: return OBJ_VAL(falconString(vm, "class", 5));
                case OBJ_LIST: return OBJ_VAL(falconString(vm, "list", 4));
                case OBJ_MAP: return OBJ_VAL(falconString(vm, "map", 3));
                case OBJ_BMETHOD: return OBJ_VAL(falconString(vm, "method", 6));
                case OBJ_CLOSURE:
                case OBJ_FUNCTION:
                case OBJ_NATIVE: return OBJ_VAL(falconString(vm, "function", 8));
                case OBJ_INSTANCE: {
                    ObjInstance *instance = AS_INSTANCE(*args);
                    char *typeStr = FALCON_ALLOCATE(vm, char, instance->class_->name->length + 6);
                    sprintf(typeStr, "class %s", instance->class_->name->chars);
                    return OBJ_VAL(falconString(vm, typeStr, instance->class_->name->length + 6));
                }
                default: break;
            }
            break;
        default: break;
    }

    return OBJ_VAL(falconString(vm, "unknown", 7)); /* Unknown type */
}

/**
 * Native Falcon function to convert a given Falcon Value to a number. The conversion is implemented
 * through the "isFalsy" function.
 */
FALCON_NATIVE(bool_) {
    ASSERT_ARGS_COUNT(vm, !=, argCount, 1);
    if (!IS_BOOL(*args)) return BOOL_VAL(!isFalsy(*args));
    return *args; /* Given value is already a boolean */
}

/**
 * Native Falcon function to convert a given Falcon Value to a number.
 */
FALCON_NATIVE(num) {
    ASSERT_ARGS_COUNT(vm, !=, argCount, 1);
    if (!IS_NUM(*args)) {
        if (!IS_STRING(*args) && !IS_BOOL(*args)) {
            interpreterError(vm, VM_ARGS_TYPE_ERR, 1, "string, boolean, or number");
            return ERR_VAL;
        }

        if (IS_STRING(*args)) { /* String to number */
            char *start = AS_CSTRING(*args), *end;
            double number = strtod(AS_CSTRING(*args), &end); /* Converts to double */

            if (start == end) { /* Checks for conversion success */
                interpreterError(vm, FALCON_CONV_STR_NUM_ERR);
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
 * Native Falcon function to convert a given Falcon Value to a string. The conversion is implemented
 * through the "valueToString" function.
 */
FALCON_NATIVE(str) {
    ASSERT_ARGS_COUNT(vm, !=, argCount, 1);
    return OBJ_VAL(valueToString(vm, args)); /* Converts value to a string */
}

/**
 * Native function to get the length of a Falcon Value (lists or strings only).
 */
FALCON_NATIVE(len) {
    ASSERT_ARGS_COUNT(vm, !=, argCount, 1);
    ASSERT_ARG_TYPE(IS_OBJ, "list, map or string", *args, vm, 1);

    /* Handles the subscript types */
    switch (AS_OBJ(*args)->type) {
        case OBJ_LIST: return NUM_VAL(AS_LIST(*args)->elements.count); /* Returns the list length */
        case OBJ_MAP: return NUM_VAL(AS_MAP(*args)->count);            /* Returns the map length */
        case OBJ_STRING: return NUM_VAL(AS_STRING(*args)->length); /* Returns the string length */
        default: interpreterError(vm, VM_ARGS_TYPE_ERR, 1, "list, map or string"); return ERR_VAL;
    }
}

/*
 * ================================================================================================
 * ================================ Class-related native functions ================================
 * ================================================================================================
 */

/**
 * Native function to test if a given Falcon Value has a given field (string).
 */
FALCON_NATIVE(hasField) {
    ASSERT_ARGS_COUNT(vm, !=, argCount, 2);
    ASSERT_ARG_TYPE(IS_INSTANCE, "class instance", args[0], vm, 1);
    ASSERT_ARG_TYPE(IS_STRING, "string", args[1], vm, 2);

    /* Checks if the field is defined */
    ObjInstance *instance = AS_INSTANCE(args[0]);
    FalconValue value;
    return BOOL_VAL(mapGet(&instance->fields, AS_STRING(args[1]), &value));
}

/**
 * Native function to get the value of a given field (string) from a given Falcon Value (class
 * instance).
 */
FALCON_NATIVE(getField) {
    ASSERT_ARGS_COUNT(vm, !=, argCount, 2);
    ASSERT_ARG_TYPE(IS_INSTANCE, "class instance", args[0], vm, 1);
    ASSERT_ARG_TYPE(IS_STRING, "string", args[1], vm, 2);

    /* Gets the field value */
    ObjInstance *instance = AS_INSTANCE(args[0]);
    FalconValue value;
    if (mapGet(&instance->fields, AS_STRING(args[1]), &value)) return value;

    /* Undefined field error */
    interpreterError(vm, VM_UNDEF_PROP_ERR, instance->class_->name->chars,
                     AS_STRING(args[1])->chars);
    return ERR_VAL;
}

/**
 * Native function to set a given Falcon Value to a given field (string) from another given Falcon
 * Value (class instance).
 */
FALCON_NATIVE(setField) {
    ASSERT_ARGS_COUNT(vm, !=, argCount, 3);
    ASSERT_ARG_TYPE(IS_INSTANCE, "class instance", args[0], vm, 1);
    ASSERT_ARG_TYPE(IS_STRING, "string", args[1], vm, 2);

    /* Sets the field value */
    ObjInstance *instance = AS_INSTANCE(args[0]);
    mapSet(vm, &instance->fields, AS_STRING(args[1]), args[2]);
    return args[2];
}

/**
 * Native function to delete a given field (string) from a given Falcon Value (class instance).
 */
FALCON_NATIVE(delField) {
    ASSERT_ARGS_COUNT(vm, !=, argCount, 2);
    ASSERT_ARG_TYPE(IS_INSTANCE, "class instance", args[0], vm, 1);
    ASSERT_ARG_TYPE(IS_STRING, "string", args[1], vm, 2);

    /* Deletes the field */
    ObjInstance *instance = AS_INSTANCE(args[0]);
    deleteFromMap(&instance->fields, AS_STRING(args[1]));
    return NULL_VAL;
}

/*
 * ================================================================================================
 * ===================================== Math native functions ====================================
 * ================================================================================================
 */

/**
 * Native Falcon function to get the absolute value of a given Falcon Value.
 */
FALCON_NATIVE(abs_) {
    ASSERT_ARGS_COUNT(vm, !=, argCount, 1);
    ASSERT_ARG_TYPE(IS_NUM, "number", *args, vm, 1);
    double absValue = fabs(AS_NUM(*args)); /* Gets the abs value */
    return NUM_VAL(absValue);
}

/**
 * Native Falcon function to get the square root of a given Falcon Value.
 */
FALCON_NATIVE(sqrt_) {
    ASSERT_ARGS_COUNT(vm, !=, argCount, 1);
    ASSERT_ARG_TYPE(IS_NUM, "number", *args, vm, 1);
    double sqrtValue = sqrt(AS_NUM(*args)); /* Gets the sqrt value */
    return NUM_VAL(sqrtValue);
}

/**
 * Native Falcon function to get the value of a number "x" to the power of a number "y".
 */
FALCON_NATIVE(pow_) {
    ASSERT_ARGS_COUNT(vm, !=, argCount, 2);
    ASSERT_ARG_TYPE(IS_NUM, "number", args[0], vm, 1);
    ASSERT_ARG_TYPE(IS_NUM, "number", args[1], vm, 2);

    double powValue = pow(AS_NUM(args[0]), AS_NUM(args[1])); /* Gets the pow value */
    return NUM_VAL(powValue);
}

/*
 * ================================================================================================
 * ====================================== IO native functions =====================================
 * ================================================================================================
 */

/**
 * Native Falcon function to prompt the user for an input and return the given input as an Falcon
 * Value.
 */
FALCON_NATIVE(input) {
    ASSERT_ARGS_COUNT(vm, >, argCount, 1);
    if (argCount == 1) {
        FalconValue prompt = *args;
        ASSERT_ARG_TYPE(IS_STRING, "string", prompt, vm, 1); /* Checks if is valid */
        printf("%s", AS_CSTRING(prompt));                    /* Prints the prompt */
    }

    char *inputString = readStrStdin(vm); /* Reads the input string */
    return OBJ_VAL(falconString(vm, inputString, strlen(inputString)));
}

/**
 * Native Falcon function to print (with a new line) a given Falcon value.
 */
FALCON_NATIVE(print) {
    if (argCount > 1) {
        for (int i = 0; i < argCount; i++) {
            printValue(vm, args[i]);
            if (i < argCount - 1) printf(" "); /* Separator */
        }
    } else {
        printValue(vm, *args);
    }

    printf("\n"); /* End */
    return NULL_VAL;
}

#undef ASSERT_ARGS_COUNT
#undef ASSERT_ARG_TYPE
#undef FALCON_NATIVE

/*
 * ================================================================================================
 * ==================================== Native functions setup ====================================
 * ================================================================================================
 */

/**
 * Defines a new native function for Falcon.
 */
static void defNative(FalconVM *vm, const char *name, FalconNativeFn function) {
    DISABLE_GC(vm); /* Avoids GC from the "defNative" ahead */
    ObjString *strName = falconString(vm, name, (int) strlen(name));
    ENABLE_GC(vm);
    DISABLE_GC(vm); /* Avoids GC from the "defNative" ahead */
    ObjNative *nativeFn = falconNative(vm, function, name);
    mapSet(vm, &vm->globals, strName, OBJ_VAL(nativeFn));
    ENABLE_GC(vm);
}

/**
 * Defines the complete set of native function for Falcon.
 */
void defineNatives(FalconVM *vm) {
    const ObjNative nativeFunctions[] = {
        /* Native functions implementations */
        {.function = lib_exit, .name = "exit"},     {.function = lib_clock, .name = "clock"},
        {.function = lib_time, .name = "time"},     {.function = type, .name = "type"},
        {.function = bool_, .name = "bool"},        {.function = num, .name = "num"},
        {.function = str, .name = "str"},           {.function = len, .name = "len"},
        {.function = hasField, .name = "hasField"}, {.function = getField, .name = "getField"},
        {.function = setField, .name = "setField"}, {.function = delField, .name = "delField"},
        {.function = abs_, .name = "abs"},          {.function = sqrt_, .name = "sqrt"},
        {.function = pow_, .name = "pow"},          {.function = input, .name = "input"},
        {.function = print, .name = "print"}};

    for (unsigned long i = 0; i < sizeof(nativeFunctions) / sizeof(nativeFunctions[0]); i++)
        defNative(vm, nativeFunctions[i].name, nativeFunctions[i].function);
}
