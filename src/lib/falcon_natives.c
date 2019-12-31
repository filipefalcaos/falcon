/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * falcon_natives.c: Falcon's native functions
 * See Falcon's license in the LICENSE file
 */

#include "falcon_natives.h"
#include "falcon_error.h"
#include "falcon_io.h"
#include "falcon_string.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Checks the validity of a given argument count */
#define CHECK_ARGS(vm, op, argCount, expectedCount)                        \
    do {                                                                   \
        if (argCount op expectedCount) {                                   \
            falconVMError(vm, VM_ARGS_COUNT_ERR, expectedCount, argCount); \
            return ERR_VAL;                                                \
        }                                                                  \
    } while (false)

/* Checks if a given value "value" of a given type "type" at a given position "pos" is a value of
 * the requested type */
#define CHECK_TYPE(type, typeName, value, vm, pos)              \
    do {                                                        \
        if (!type(value)) {                                     \
            falconVMError(vm, VM_ARGS_TYPE_ERR, pos, typeName); \
            return ERR_VAL;                                     \
        }                                                       \
    } while (false)

/* Defines a common interface to all Falcon native functions */
#define FALCON_NATIVE(name) static FalconValue name(FalconVM *vm, int argCount, FalconValue *args)

/*
 * ================================================================================================
 * ================================ System-related native functions ===============================
 * ================================================================================================
 */

/**
 * Native Falcon function to print Falcon's authors.
 */
FALCON_NATIVE(authors) {
    (void) args; /* Unused */
    CHECK_ARGS(vm, !=, argCount, 0);
    falconPrintAuthors();
    return NULL_VAL;
}

/**
 * Native Falcon function to print Falcon's MIT license.
 */
FALCON_NATIVE(license) {
    (void) args; /* Unused */
    CHECK_ARGS(vm, !=, argCount, 0);
    falconPrintLicense();
    return NULL_VAL;
}

/**
 * Native Falcon function to print interpreter usage details.
 */
FALCON_NATIVE(help) {
    (void) args; /* Unused */
    CHECK_ARGS(vm, !=, argCount, 0);
    falconPrintUsage();
    return NULL_VAL;
}

/**
 * Native Falcon function to exit the running process with a given exit code.
 */
FALCON_NATIVE(exit_) {
    CHECK_ARGS(vm, !=, argCount, 1);
    CHECK_TYPE(IS_NUM, "number", *args, vm, 1);
    exit((int) AS_NUM(*args)); /* Exits the process */
}

/**
 * Native Falcon function to compute the elapsed time since the program started running, in seconds.
 */
FALCON_NATIVE(clock_) {
    (void) args; /* Unused */
    CHECK_ARGS(vm, !=, argCount, 0);
    return NUM_VAL((double) clock() / CLOCKS_PER_SEC);
}

/**
 * Native Falcon function to compute the UNIX timestamp, in seconds.
 */
FALCON_NATIVE(time_) {
    (void) args; /* Unused */
    CHECK_ARGS(vm, !=, argCount, 0);
    return NUM_VAL((double) time(NULL));
}

/*
 * ================================================================================================
 * ================================= Type-related native functions ================================
 * ================================================================================================
 */

/**
 * Native Falcon function to get the type of a given Falcon Value, as a string.
 */
FALCON_NATIVE(type) {
    CHECK_ARGS(vm, !=, argCount, 1);
    char *typeString = NULL;
    size_t typeStringLen = 0;

    /* Checks the value type */
    switch (args->type) {
        case VAL_BOOL:
            typeString = "<bool>";
            typeStringLen = 6;
            break;
        case VAL_NULL:
            typeString = "<null>";
            typeStringLen = 6;
            break;
        case VAL_NUM:
            typeString = "<number>";
            typeStringLen = 8;
            break;
        case VAL_OBJ:
            switch (OBJ_TYPE(*args)) {
                case OBJ_STRING:
                    typeString = "<string>";
                    typeStringLen = 8;
                    break;
                case OBJ_LIST:
                    typeString = "<list>";
                    typeStringLen = 6;
                    break;
                case OBJ_CLOSURE:
                case OBJ_FUNCTION:
                case OBJ_NATIVE:
                    typeString = "<function>";
                    typeStringLen = 10;
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }

    return OBJ_VAL(copyString(vm, typeString, typeStringLen));
}

/**
 * Native Falcon function to convert a given Falcon Value to a number. The conversion is implemented
 * through the "isFalsy" function.
 */
FALCON_NATIVE(bool_) {
    CHECK_ARGS(vm, !=, argCount, 1);
    if (!IS_BOOL(*args)) return BOOL_VAL(!isFalsy(*args));
    return *args; /* Given value is already a boolean */
}

/**
 * Native Falcon function to convert a given Falcon Value to a number.
 */
FALCON_NATIVE(num) {
    CHECK_ARGS(vm, !=, argCount, 1);
    if (!IS_NUM(*args)) {
        if (!IS_STRING(*args) && !IS_BOOL(*args)) {
            falconVMError(vm, VM_ARGS_TYPE_ERR, 1, "string, boolean, or number");
            return ERR_VAL;
        }

        if (IS_STRING(*args)) { /* String to number */
            char *start = AS_CSTRING(*args), *end;
            double number = strtod(AS_CSTRING(*args), &end); /* Converts to double */

            if (start == end) { /* Checks for conversion success */
                falconVMError(vm, FALCON_CONV_STR_NUM_ERR);
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
    CHECK_ARGS(vm, !=, argCount, 1);
    if (!IS_STRING(*args)) {
        char *string = valueToString(vm, args); /* Converts value to a string */
        return OBJ_VAL(copyString(vm, string, strlen(string)));
    }

    return *args; /* Given value is already a string */
}

/**
 * Native function to get the length of a Falcon Value (lists only).
 */
FALCON_NATIVE(len) {
    CHECK_ARGS(vm, !=, argCount, 1);
    CHECK_TYPE(IS_LIST, "list", *args, vm, 1);
    return NUM_VAL(AS_LIST(*args)->elements.count); /* Returns list length */
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
    CHECK_ARGS(vm, !=, argCount, 1);
    CHECK_TYPE(IS_NUM, "number", *args, vm, 1);
    double absValue = fabs(AS_NUM(*args)); /* Gets the abs value */
    return NUM_VAL(absValue);
}

/**
 * Native Falcon function to get the square root of a given Falcon Value.
 */
FALCON_NATIVE(sqrt_) {
    CHECK_ARGS(vm, !=, argCount, 1);
    CHECK_TYPE(IS_NUM, "number", *args, vm, 1);
    double sqrtValue = sqrt(AS_NUM(*args)); /* Gets the sqrt value */
    return NUM_VAL(sqrtValue);
}

/**
 * Native Falcon function to get the value of a number "x" to the power of a number "y".
 */
FALCON_NATIVE(pow_) {
    CHECK_ARGS(vm, !=, argCount, 2);
    CHECK_TYPE(IS_NUM, "number", args[0], vm, 1);
    CHECK_TYPE(IS_NUM, "number", args[1], vm, 2);

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
    CHECK_ARGS(vm, >, argCount, 1);
    if (argCount == 1) {
        FalconValue prompt = *args;
        CHECK_TYPE(IS_STRING, "string", prompt, vm, 1); /* Checks if is valid */
        printf("%s", AS_CSTRING(prompt));               /* Prints the prompt */
    }

    char *inputString = readStrStdin(vm); /* Reads the input string */
    return OBJ_VAL(copyString(vm, inputString, strlen(inputString)));
}

/**
 * Native Falcon function to print (with a new line) a given Falcon value.
 */
FALCON_NATIVE(print) {
    if (argCount > 1) {
        for (int i = 0; i < argCount; i++) {
            printValue(vm, args[i], false);
            if (i < argCount - 1) printf(" "); /* Separator */
        }
    } else {
        printValue(vm, *args, false);
    }

    printf("\n"); /* End */
    return NULL_VAL;
}

#undef CHECK_ARGS
#undef CHECK_TYPE
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
    VMPush(vm, OBJ_VAL(copyString(vm, name, (int) strlen(name)))); /* Avoids GC */
    VMPush(vm, OBJ_VAL(falconNative(vm, function, name)));         /* Avoids GC */
    tableSet(vm, &vm->globals, AS_STRING(vm->stack[0]), vm->stack[1]);
    VMPop(vm);
    VMPop(vm);
}

/**
 * Defines the complete set of native function for Falcon.
 */
void falconDefNatives(FalconVM *vm) {
    const ObjNative nativeFunctions[] = { /* Native functions implementations */
        { .function = authors, .name = "authors" },
        { .function = license, .name = "license" },
        { .function = help, .name = "help" },
        { .function = exit_, .name = "exit" },
        { .function = clock_, .name = "clock" },
        { .function = time_, .name = "time" },
        { .function = type, .name = "type" },
        { .function = bool_, .name = "bool" },
        { .function = num, .name = "num" },
        { .function = str, .name = "str" },
        { .function = len, .name = "len" },
        { .function = abs_, .name = "abs" },
        { .function = sqrt_, .name = "sqrt" },
        { .function = pow_, .name = "pow" },
        { .function = input, .name = "input" },
        { .function = print, .name = "print" }
    };

    /* Define listed native functions */
    for (unsigned long i = 0; i < sizeof(nativeFunctions) / sizeof(nativeFunctions[0]); i++)
        defNative(vm, nativeFunctions[i].name, nativeFunctions[i].function);
}
