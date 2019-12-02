/*
 * YAPL version 0.0.1 (master, Oct 18 2019)
 * yapl_natives.c: YAPL's native functions
 * See YAPL's license in the LICENSE file
 */

#include "yapl_natives.h"
#include "../vm/yapl_memmanager.h"
#include "io/yapl_io.h"
#include "math/yapl_math.h"
#include "string/yapl_string.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Checks the validity of a given argument count */
#define CHECK_ARGS(vm, op, argCount, expectedCount)               \
    do {                                                          \
        if (argCount op expectedCount) {                          \
            VMError(vm, ARGS_COUNT_ERR, expectedCount, argCount); \
            return ERR_VAL;                                       \
        }                                                         \
    } while (false)

/* Defines a common interface to all YAPL native functions */
#define YAPL_NATIVE(name) static Value name(VM *vm, int argCount, Value *args)

/*
 * ================================================================================================
 * ================================ System-related native functions ===============================
 * ================================================================================================
 */

/**
 * Native YAPL function to print YAPL's authors.
 */
YAPL_NATIVE(authorsNative) {
    (void) args; /* Unused */
    CHECK_ARGS(vm, !=, argCount, 0);
    printAuthors();
    return NULL_VAL;
}

/**
 * Native YAPL function to print YAPL's MIT license.
 */
YAPL_NATIVE(licenseNative) {
    (void) args; /* Unused */
    CHECK_ARGS(vm, !=, argCount, 0);
    printLicense();
    return NULL_VAL;
}

/**
 * Native YAPL function to print interpreter usage details.
 */
YAPL_NATIVE(helpNative) {
    (void) args; /* Unused */
    CHECK_ARGS(vm, !=, argCount, 0);
    printUsage();
    return NULL_VAL;
}

/**
 * Native YAPL function to exit the running process with a given exit code.
 */
YAPL_NATIVE(exitNative) {
    CHECK_ARGS(vm, !=, argCount, 1);
    if (!IS_NUM(*args)) {
        VMError(vm, ARGS_TYPE_ERR, 1, "number");
        return ERR_VAL;
    }

    exit((int) AS_NUM(*args)); /* Exits the process */
}

/**
 * Native YAPL function to compute the elapsed time since the program started running, in seconds.
 */
YAPL_NATIVE(clockNative) {
    (void) args; /* Unused */
    CHECK_ARGS(vm, !=, argCount, 0);
    return NUM_VAL((double) clock() / CLOCKS_PER_SEC);
}

/**
 * Native YAPL function to compute the UNIX timestamp, in seconds.
 */
YAPL_NATIVE(timeNative) {
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
 * Native YAPL function to get the type of a given YAPL Value, as a string.
 */
YAPL_NATIVE(typeNative) {
    CHECK_ARGS(vm, !=, argCount, 1);
    char *typeString = NULL;
    int typeStringLen = 0;

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
 * Native YAPL function to convert a given YAPL Value to a number. The conversion is implemented
 * through the "isFalsey" function.
 */
YAPL_NATIVE(boolNative) {
    CHECK_ARGS(vm, !=, argCount, 1);
    if (!IS_BOOL(*args)) {
        return BOOL_VAL(!isFalsey(*args));
    }

    return *args; /* Given value is already a boolean */
}

/**
 * Native YAPL function to convert a given YAPL Value to a number.
 */
YAPL_NATIVE(numNative) {
    CHECK_ARGS(vm, !=, argCount, 1);
    if (!IS_NUM(*args)) {
        if (!IS_STRING(*args) && !IS_BOOL(*args)) {
            VMError(vm, ARGS_TYPE_ERR, 1, "string, boolean, or number");
            return ERR_VAL;
        }

        if (IS_STRING(*args)) { /* String to number */
            char *start = AS_CLANG_STRING(*args), *end;
            double number = strtod(AS_CLANG_STRING(*args), &end); /* Converts to double */

            if (start == end) { /* Checks for conversion success */
                VMError(vm, CONV_STR_NUM_ERR);
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
 * Native YAPL function to convert a given YAPL Value to a string. The conversion is implemented
 * through the "valueToString" function.
 */
YAPL_NATIVE(strNative) {
    CHECK_ARGS(vm, !=, argCount, 1);
    if (!IS_STRING(*args)) {
        char *string = valueToString(args); /* Converts value to a string */
        return OBJ_VAL(copyString(vm, string, strlen(string)));
    }

    return *args; /* Given value is already a string */
}

/*
 * ================================================================================================
 * ===================================== Math native functions ====================================
 * ================================================================================================
 */

/**
 * Native YAPL function to get the absolute value of a given YAPL Value.
 */
YAPL_NATIVE(absNative) {
    CHECK_ARGS(vm, !=, argCount, 1);
    if (!IS_NUM(*args)) {
        VMError(vm, ARGS_TYPE_ERR, 1, "number");
        return ERR_VAL;
    }

    double absValue = getAbs(AS_NUM(*args)); /* Gets the abs value */
    return NUM_VAL(absValue);
}

/**
 * Native YAPL function to get the absolute value of a given YAPL Value.
 */
YAPL_NATIVE(sqrtNative) {
    CHECK_ARGS(vm, !=, argCount, 1);
    if (!IS_NUM(*args)) {
        VMError(vm, ARGS_TYPE_ERR, 1, "number");
        return ERR_VAL;
    }

    double sqrtValue = getSqrt(AS_NUM(*args)); /* Gets the sqrt value */
    return NUM_VAL(sqrtValue);
}

/*
 * ================================================================================================
 * ====================================== IO native functions =====================================
 * ================================================================================================
 */

/**
 * Native YAPL function to prompt the user for an input and return the given input as an YAPL Value.
 */
YAPL_NATIVE(inputNative) {
    CHECK_ARGS(vm, >, argCount, 1);

    if (argCount == 1) {
        Value prompt = *args;
        if (!IS_STRING(prompt)) { /* Checks if the prompt is valid */
            VMError(vm, ARGS_TYPE_ERR, 1, "string");
            return ERR_VAL;
        }

        printf("%s", AS_CLANG_STRING(prompt)); /* Prints the prompt */
    }

    char *inputString = readStrStdin(); /* Reads the input string */
    return OBJ_VAL(copyString(vm, inputString, strlen(inputString)));
}

/**
 * Native YAPL function to print an YAPL value.
 */
YAPL_NATIVE(printNative) {
    CHECK_ARGS(vm, !=, argCount, 1);
    printValue(*args);
    return NULL_VAL;
}

/**
 * Native YAPL function to print (with a new line) an YAPL value.
 */
YAPL_NATIVE(printlnNative) {
    CHECK_ARGS(vm, !=, argCount, 1);
    printValue(*args);
    printf("\n");
    return NULL_VAL;
}

#undef CHECK_ARGS

/*
 * ================================================================================================
 * ==================================== Native functions setup ====================================
 * ================================================================================================
 */

/**
 * Allocates a new YAPL native function object.
 */
static ObjNative *newNative(VM *vm, NativeFn function) {
    ObjNative *native = ALLOCATE_OBJ(vm, ObjNative, OBJ_NATIVE);
    native->function = function;
    return native;
}

/**
 * Defines a new native function for YAPL.
 */
static void defineNative(VM *vm, const char *name, NativeFn function) {
    push(vm, OBJ_VAL(copyString(vm, name, (int) strlen(name))));
    push(vm, OBJ_VAL(newNative(vm, function)));
    tableSet(&vm->globals, AS_STRING(vm->stack[0]), vm->stack[1]);
    pop(vm);
    pop(vm);
}

/**
 * Defines the complete set of native function for YAPL.
 */
void defineNatives(VM *vm) {
    const NativeFnImp nativeFunctions[] = { /* Native functions implementations */
        { "authors", authorsNative },
        { "license", licenseNative },
        { "help", helpNative },
        { "exit", exitNative },
        { "clock", clockNative },
        { "time", timeNative },
        { "type", typeNative },
        { "bool", boolNative },
        { "num", numNative },
        { "str", strNative },
        { "abs", absNative },
        { "sqrt", sqrtNative },
        { "input", inputNative },
        { "print", printNative },
        { "println", printlnNative }
    };

    /* Define listed native functions */
    for (unsigned long i = 0; i < sizeof(nativeFunctions) / sizeof(nativeFunctions[0]); i++)
        defineNative(vm, nativeFunctions[i].functionName, nativeFunctions[i].nativeFn);
}
