/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * falcon_natives.c: Falcon's native functions
 * See Falcon's license in the LICENSE file
 */

#include "falcon_natives.h"
#include "io/falcon_io.h"
#include "math/falcon_math.h"
#include "string/falcon_string.h"
#include "../vm/falcon_object.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Checks the validity of a given argument count */
#define FALCON_CHECK_ARGS(vm, op, argCount, expectedCount)                     \
    do {                                                                       \
        if (argCount op expectedCount) {                                       \
            FalconVMError(vm, FALCON_ARGS_COUNT_ERR, expectedCount, argCount); \
            return FALCON_ERR_VAL;                                             \
        }                                                                      \
    } while (false)

/* Checks if a given value "value" of a given type "type" at a given position "pos" is a value of
 * the requested type */
#define FALCON_CHECK_TYPE(type, typeName, value, vm, pos)           \
    do {                                                            \
        if (!type(value)) {                                         \
            FalconVMError(vm, FALCON_ARGS_TYPE_ERR, pos, typeName); \
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
FALCON_NATIVE(FalconAuthorsNative) {
    (void) args; /* Unused */
    FALCON_CHECK_ARGS(vm, !=, argCount, 0);
    FalconPrintAuthors();
    return FALCON_NULL_VAL;
}

/**
 * Native Falcon function to print Falcon's MIT license.
 */
FALCON_NATIVE(FalconLicenseNative) {
    (void) args; /* Unused */
    FALCON_CHECK_ARGS(vm, !=, argCount, 0);
    FalconPrintLicense();
    return FALCON_NULL_VAL;
}

/**
 * Native Falcon function to print interpreter usage details.
 */
FALCON_NATIVE(FalconHelpNative) {
    (void) args; /* Unused */
    FALCON_CHECK_ARGS(vm, !=, argCount, 0);
    FalconPrintUsage();
    return FALCON_NULL_VAL;
}

/**
 * Native Falcon function to exit the running process with a given exit code.
 */
FALCON_NATIVE(FalconExitNative) {
    FALCON_CHECK_ARGS(vm, !=, argCount, 1);
    FALCON_CHECK_TYPE(FALCON_IS_NUM, "number", *args, vm, 1);
    exit((int) FALCON_AS_NUM(*args)); /* Exits the process */
}

/**
 * Native Falcon function to compute the elapsed time since the program started running, in seconds.
 */
FALCON_NATIVE(FalconClockNative) {
    (void) args; /* Unused */
    FALCON_CHECK_ARGS(vm, !=, argCount, 0);
    return FALCON_NUM_VAL((double) clock() / CLOCKS_PER_SEC);
}

/**
 * Native Falcon function to compute the UNIX timestamp, in seconds.
 */
FALCON_NATIVE(FalconTimeNative) {
    (void) args; /* Unused */
    FALCON_CHECK_ARGS(vm, !=, argCount, 0);
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
FALCON_NATIVE(FalconTypeNative) {
    FALCON_CHECK_ARGS(vm, !=, argCount, 1);
    char *typeString = NULL;
    int typeStringLen = 0;

    /* Checks the value type */
    switch (args->type) {
        case FALCON_VAL_BOOL:
            typeString = "<bool>";
            typeStringLen = 6;
            break;
        case FALCON_VAL_NULL:
            typeString = "<null>";
            typeStringLen = 6;
            break;
        case FALCON_VAL_NUM:
            typeString = "<number>";
            typeStringLen = 8;
            break;
        case FALCON_VAL_OBJ:
            switch (FALCON_OBJ_TYPE(*args)) {
                case FALCON_OBJ_STRING:
                    typeString = "<string>";
                    typeStringLen = 8;
                    break;
                case FALCON_OBJ_CLOSURE:
                case FALCON_OBJ_FUNCTION:
                case FALCON_OBJ_NATIVE:
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

    return FALCON_OBJ_VAL(FalconCopyString(vm, typeString, typeStringLen));
}

/**
 * Native Falcon function to convert a given Falcon Value to a number. The conversion is implemented
 * through the "FalconIsFalsey" function.
 */
FALCON_NATIVE(FalconBoolNative) {
    FALCON_CHECK_ARGS(vm, !=, argCount, 1);
    if (!FALCON_IS_BOOL(*args)) return FALCON_BOOL_VAL(!FalconIsFalsey(*args));
    return *args; /* Given value is already a boolean */
}

/**
 * Native Falcon function to convert a given Falcon Value to a number.
 */
FALCON_NATIVE(FalconNumNative) {
    FALCON_CHECK_ARGS(vm, !=, argCount, 1);
    if (!FALCON_IS_NUM(*args)) {
        if (!FALCON_IS_STRING(*args) && !FALCON_IS_BOOL(*args)) {
            FalconVMError(vm, FALCON_ARGS_TYPE_ERR, 1, "string, boolean, or number");
            return FALCON_ERR_VAL;
        }

        if (FALCON_IS_STRING(*args)) { /* String to number */
            char *start = FALCON_AS_CSTRING(*args), *end;
            double number = strtod(FALCON_AS_CSTRING(*args), &end); /* Converts to double */

            if (start == end) { /* Checks for conversion success */
                FalconVMError(vm, FALCON_CONV_STR_NUM_ERR);
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
 * through the "FalconValueToString" function.
 */
FALCON_NATIVE(FalconStrNative) {
    FALCON_CHECK_ARGS(vm, !=, argCount, 1);
    if (!FALCON_IS_STRING(*args)) {
        char *string = FalconValueToString(vm, args); /* Converts value to a string */
        return FALCON_OBJ_VAL(FalconCopyString(vm, string, strlen(string)));
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
FALCON_NATIVE(FalconAbsNative) {
    FALCON_CHECK_ARGS(vm, !=, argCount, 1);
    FALCON_CHECK_TYPE(FALCON_IS_NUM, "number", *args, vm, 1);
    double absValue = FalconAbs(FALCON_AS_NUM(*args)); /* Gets the abs value */
    return FALCON_NUM_VAL(absValue);
}

/**
 * Native Falcon function to get the square root of a given Falcon Value.
 */
FALCON_NATIVE(FalconSqrtNative) {
    FALCON_CHECK_ARGS(vm, !=, argCount, 1);
    FALCON_CHECK_TYPE(FALCON_IS_NUM, "number", *args, vm, 1);
    double sqrtValue = FalconSqrt(FALCON_AS_NUM(*args)); /* Gets the sqrt value */
    return FALCON_NUM_VAL(sqrtValue);
}

/**
 * Native Falcon function to get the value of a number "x" to the power of a number "y".
 */
FALCON_NATIVE(FalconPowNative) {
    FALCON_CHECK_ARGS(vm, !=, argCount, 2);
    FALCON_CHECK_TYPE(FALCON_IS_NUM, "number", args[0], vm, 1);
    FALCON_CHECK_TYPE(FALCON_IS_NUM, "number", args[1], vm, 2);

    double powValue =
        FalconPow(FALCON_AS_NUM(args[0]), FALCON_AS_NUM(args[1])); /* Gets the pow value */
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
FALCON_NATIVE(FalconInputNative) {
    FALCON_CHECK_ARGS(vm, >, argCount, 1);
    if (argCount == 1) {
        FalconValue prompt = *args;
        FALCON_CHECK_TYPE(FALCON_IS_STRING, "string", prompt, vm, 1); /* Checks if is valid */
        printf("%s", FALCON_AS_CSTRING(prompt));                      /* Prints the prompt */
    }

    char *inputString = FalconReadStrStdin(vm); /* Reads the input string */
    return FALCON_OBJ_VAL(FalconCopyString(vm, inputString, strlen(inputString)));
}

/**
 * Native Falcon function to print (with a new line) a given Falcon value.
 */
FALCON_NATIVE(FalconPrintNative) {
    if (argCount > 1) {
        for (int i = 0; i < argCount; i++) {
            FalconPrintValue(args[i]);
            if (i < argCount - 1) printf(" "); /* Separator */
        }
    } else {
        FalconPrintValue(*args);
    }

    printf("\n"); /* End */
    return FALCON_NULL_VAL;
}

#undef FALCON_CHECK_ARGS

/*
 * ================================================================================================
 * ==================================== Native functions setup ====================================
 * ================================================================================================
 */

/**
 * Defines a new native function for Falcon.
 */
static void defineNative(FalconVM *vm, const char *name, FalconNativeFn function) {
    FalconPush(vm, FALCON_OBJ_VAL(FalconCopyString(vm, name, (int) strlen(name))));
    FalconPush(vm, FALCON_OBJ_VAL(FalconNewNative(vm, function, name)));
    FalconTableSet(vm, &vm->globals, FALCON_AS_STRING(vm->stack[0]), vm->stack[1]);
    FalconPop(vm);
    FalconPop(vm);
}

/**
 * Defines the complete set of native function for Falcon.
 */
void FalconDefineNatives(FalconVM *vm) {
    const FalconNativeFnImp nativeFunctions[] = { /* Native functions implementations */
        { "authors", FalconAuthorsNative },
        { "license", FalconLicenseNative },
        { "help", FalconHelpNative },
        { "exit", FalconExitNative },
        { "clock", FalconClockNative },
        { "time", FalconTimeNative },
        { "type", FalconTypeNative },
        { "bool", FalconBoolNative },
        { "num", FalconNumNative },
        { "str", FalconStrNative },
        { "abs", FalconAbsNative },
        { "sqrt", FalconSqrtNative },
        { "pow", FalconPowNative },
        { "input", FalconInputNative },
        { "print", FalconPrintNative }
    };

    /* Define listed native functions */
    for (unsigned long i = 0; i < sizeof(nativeFunctions) / sizeof(nativeFunctions[0]); i++)
        defineNative(vm, nativeFunctions[i].functionName, nativeFunctions[i].nativeFn);
}
