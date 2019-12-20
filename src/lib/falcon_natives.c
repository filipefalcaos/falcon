/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * falcon_natives.c: Falcon's native functions
 * See Falcon's license in the LICENSE file
 */

#include "falcon_natives.h"
#include "falcon_io.h"
#include "falcon_math.h"
#include "falcon_string.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Checks the validity of a given argument count */
#define CHECK_ARGS(vm, op, argCount, expectedCount)                            \
    do {                                                                       \
        if (argCount op expectedCount) {                                       \
            falconVMError(vm, FALCON_ARGS_COUNT_ERR, expectedCount, argCount); \
            return FALCON_ERR_VAL;                                             \
        }                                                                      \
    } while (false)

/* Checks if a given value "value" of a given type "type" at a given position "pos" is a value of
 * the requested type */
#define CHECK_TYPE(type, typeName, value, vm, pos)                  \
    do {                                                            \
        if (!type(value)) {                                         \
            falconVMError(vm, FALCON_ARGS_TYPE_ERR, pos, typeName); \
            return FALCON_ERR_VAL;                                  \
        }                                                           \
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
FALCON_NATIVE(falconAuthorsNative) {
    (void) args; /* Unused */
    CHECK_ARGS(vm, !=, argCount, 0);
    falconPrintAuthors();
    return FALCON_NULL_VAL;
}

/**
 * Native Falcon function to print Falcon's MIT license.
 */
FALCON_NATIVE(falconLicenseNative) {
    (void) args; /* Unused */
    CHECK_ARGS(vm, !=, argCount, 0);
    falconPrintLicense();
    return FALCON_NULL_VAL;
}

/**
 * Native Falcon function to print interpreter usage details.
 */
FALCON_NATIVE(falconHelpNative) {
    (void) args; /* Unused */
    CHECK_ARGS(vm, !=, argCount, 0);
    falconPrintUsage();
    return FALCON_NULL_VAL;
}

/**
 * Native Falcon function to exit the running process with a given exit code.
 */
FALCON_NATIVE(falconExitNative) {
    CHECK_ARGS(vm, !=, argCount, 1);
    CHECK_TYPE(FALCON_IS_NUM, "number", *args, vm, 1);
    exit((int) FALCON_AS_NUM(*args)); /* Exits the process */
}

/**
 * Native Falcon function to compute the elapsed time since the program started running, in seconds.
 */
FALCON_NATIVE(falconClockNative) {
    (void) args; /* Unused */
    CHECK_ARGS(vm, !=, argCount, 0);
    return FALCON_NUM_VAL((double) clock() / CLOCKS_PER_SEC);
}

/**
 * Native Falcon function to compute the UNIX timestamp, in seconds.
 */
FALCON_NATIVE(falconTimeNative) {
    (void) args; /* Unused */
    CHECK_ARGS(vm, !=, argCount, 0);
    return FALCON_NUM_VAL((double) time(NULL));
}

/*
 * ================================================================================================
 * ================================= Type-related native functions ================================
 * ================================================================================================
 */

/**
 * Native Falcon function to get the type of a given Falcon Value, as a string.
 */
FALCON_NATIVE(falconTypeNative) {
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
            switch (FALCON_OBJ_TYPE(*args)) {
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

    return FALCON_OBJ_VAL(falconCopyString(vm, typeString, typeStringLen));
}

/**
 * Native Falcon function to convert a given Falcon Value to a number. The conversion is implemented
 * through the "falconIsFalsy" function.
 */
FALCON_NATIVE(falconBoolNative) {
    CHECK_ARGS(vm, !=, argCount, 1);
    if (!FALCON_IS_BOOL(*args)) return FALCON_BOOL_VAL(!falconIsFalsy(*args));
    return *args; /* Given value is already a boolean */
}

/**
 * Native Falcon function to convert a given Falcon Value to a number.
 */
FALCON_NATIVE(falconNumNative) {
    CHECK_ARGS(vm, !=, argCount, 1);
    if (!FALCON_IS_NUM(*args)) {
        if (!FALCON_IS_STRING(*args) && !FALCON_IS_BOOL(*args)) {
            falconVMError(vm, FALCON_ARGS_TYPE_ERR, 1, "string, boolean, or number");
            return FALCON_ERR_VAL;
        }

        if (FALCON_IS_STRING(*args)) { /* String to number */
            char *start = FALCON_AS_CSTRING(*args), *end;
            double number = strtod(FALCON_AS_CSTRING(*args), &end); /* Converts to double */

            if (start == end) { /* Checks for conversion success */
                falconVMError(vm, FALCON_CONV_STR_NUM_ERR);
                return FALCON_ERR_VAL;
            }

            return FALCON_NUM_VAL(number);
        } else { /* Boolean to number */
            return FALCON_NUM_VAL(FALCON_AS_BOOL(*args) ? 1 : 0);
        }
    }

    return *args; /* Given value is already a number */
}

/**
 * Native Falcon function to convert a given Falcon Value to a string. The conversion is implemented
 * through the "falconValToString" function.
 */
FALCON_NATIVE(falconStrNative) {
    CHECK_ARGS(vm, !=, argCount, 1);
    if (!FALCON_IS_STRING(*args)) {
        char *string = falconValToString(vm, args); /* Converts value to a string */
        return FALCON_OBJ_VAL(falconCopyString(vm, string, strlen(string)));
    }

    return *args; /* Given value is already a string */
}

/*
 * ================================================================================================
 * ===================================== Math native functions ====================================
 * ================================================================================================
 */

/**
 * Native Falcon function to get the absolute value of a given Falcon Value.
 */
FALCON_NATIVE(falconAbsNative) {
    CHECK_ARGS(vm, !=, argCount, 1);
    CHECK_TYPE(FALCON_IS_NUM, "number", *args, vm, 1);
    double absValue = falconAbs(FALCON_AS_NUM(*args)); /* Gets the abs value */
    return FALCON_NUM_VAL(absValue);
}

/**
 * Native Falcon function to get the square root of a given Falcon Value.
 */
FALCON_NATIVE(falconSqrtNative) {
    CHECK_ARGS(vm, !=, argCount, 1);
    CHECK_TYPE(FALCON_IS_NUM, "number", *args, vm, 1);
    double sqrtValue = falconSqrt(FALCON_AS_NUM(*args)); /* Gets the sqrt value */
    return FALCON_NUM_VAL(sqrtValue);
}

/**
 * Native Falcon function to get the value of a number "x" to the power of a number "y".
 */
FALCON_NATIVE(falconPowNative) {
    CHECK_ARGS(vm, !=, argCount, 2);
    CHECK_TYPE(FALCON_IS_NUM, "number", args[0], vm, 1);
    CHECK_TYPE(FALCON_IS_NUM, "number", args[1], vm, 2);

    double powValue =
        falconPow(FALCON_AS_NUM(args[0]), FALCON_AS_NUM(args[1])); /* Gets the pow value */
    return FALCON_NUM_VAL(powValue);
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
FALCON_NATIVE(falconInputNative) {
    CHECK_ARGS(vm, >, argCount, 1);
    if (argCount == 1) {
        FalconValue prompt = *args;
        CHECK_TYPE(FALCON_IS_STRING, "string", prompt, vm, 1); /* Checks if is valid */
        printf("%s", FALCON_AS_CSTRING(prompt));               /* Prints the prompt */
    }

    char *inputString = falconReadStrStdin(vm); /* Reads the input string */
    return FALCON_OBJ_VAL(falconCopyString(vm, inputString, strlen(inputString)));
}

/**
 * Native Falcon function to print (with a new line) a given Falcon value.
 */
FALCON_NATIVE(falconPrintNative) {
    if (argCount > 1) {
        for (int i = 0; i < argCount; i++) {
            falconPrintVal(vm, args[i], false);
            if (i < argCount - 1) printf(" "); /* Separator */
        }
    } else {
        falconPrintVal(vm, *args, false);
    }

    printf("\n"); /* End */
    return FALCON_NULL_VAL;
}

#undef CHECK_ARGS

/*
 * ================================================================================================
 * ==================================== Native functions setup ====================================
 * ================================================================================================
 */

/**
 * Defines a new native function for Falcon.
 */
static void defNative(FalconVM *vm, const char *name, FalconNativeFn function) {
    falconPush(vm, FALCON_OBJ_VAL(falconCopyString(vm, name, (int) strlen(name)))); /* Avoids GC */
    falconPush(vm, FALCON_OBJ_VAL(falconNative(vm, function, name)));               /* Avoids GC */
    falconTableSet(vm, &vm->globals, FALCON_AS_STRING(vm->stack[0]), vm->stack[1]);
    falconPop(vm);
    falconPop(vm);
}

/**
 * Defines the complete set of native function for Falcon.
 */
void falconDefNatives(FalconVM *vm) {
    const ObjNative nativeFunctions[] = { /* Native functions implementations */
        { .function = falconAuthorsNative, .name = "authors" },
        { .function = falconLicenseNative, .name = "license" },
        { .function = falconHelpNative, .name = "help" },
        { .function = falconExitNative, .name = "exit" },
        { .function = falconClockNative, .name = "clock" },
        { .function = falconTimeNative, .name = "time" },
        { .function = falconTypeNative, .name = "type" },
        { .function = falconBoolNative, .name = "bool" },
        { .function = falconNumNative, .name = "num" },
        { .function = falconStrNative, .name = "str" },
        { .function = falconAbsNative, .name = "abs" },
        { .function = falconSqrtNative, .name = "sqrt" },
        { .function = falconPowNative, .name = "pow" },
        { .function = falconInputNative, .name = "input" },
        { .function = falconPrintNative, .name = "print" }
    };

    /* Define listed native functions */
    for (unsigned long i = 0; i < sizeof(nativeFunctions) / sizeof(nativeFunctions[0]); i++)
        defNative(vm, nativeFunctions[i].name, nativeFunctions[i].function);
}
