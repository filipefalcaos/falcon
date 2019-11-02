/*
 * YAPL version 0.0.1 (master, Oct 18 2019)
 * yapl_natives.c: YAPL's native functions
 * See YAPL's license in the LICENSE file
 */

#include "yapl_natives.h"
#include "io/yapl_io.h"
#include "string/yapl_string.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Begin CHECK_ARGS - Checks the validity of a given argument count */
#define CHECK_ARGS(op, argCount, expectedCount)               \
    do {                                                      \
        if (argCount op expectedCount) {                      \
            VMError(ARGS_COUNT_ERR, expectedCount, argCount); \
            return ERR_VAL;                                   \
        }                                                     \
    } while (false)

/*
 * ================================================================================================
 * ================================ System-related native functions ===============================
 * ================================================================================================
 */

/**
 * Native YAPL function to print YAPL's authors.
 */
static Value authorsNative(int argCount, Value *args) {
    CHECK_ARGS(!=, argCount, 0);
    printAuthors();
    return NULL_VAL;
}

/**
 * Native YAPL function to print YAPL's MIT license.
 */
static Value licenseNative(int argCount, Value *args) {
    CHECK_ARGS(!=, argCount, 0);
    printLicense();
    return NULL_VAL;
}

/**
 * Native YAPL function to print interpreter usage details.
 */
static Value helpNative(int argCount, Value *args) {
    CHECK_ARGS(!=, argCount, 0);
    printUsage();
    return NULL_VAL;
}

/**
 * Native YAPL function to exit the running process with a given exit code.
 */
static Value exitNative(int argCount, Value *args) {
    CHECK_ARGS(!=, argCount, 1);
    if (!IS_NUM(*args)) {
        VMError(ARGS_TYPE_ERR, 1, "number");
        return NULL_VAL;
    }

    exit((int) AS_NUM(*args)); /* Exits the process */
}

/**
 * Native YAPL function to compute the elapsed time since the program started running, in seconds.
 */
static Value clockNative(int argCount, Value *args) {
    CHECK_ARGS(!=, argCount, 0);
    return NUM_VAL((double) clock() / CLOCKS_PER_SEC);
}

/**
 * Native YAPL function to compute the UNIX timestamp, in seconds.
 */
static Value timeNative(int argCount, Value *args) {
    CHECK_ARGS(!=, argCount, 0);
    return NUM_VAL((double) time(NULL));
}

/*
 * ================================================================================================
 * ====================================== IO native functions =====================================
 * ================================================================================================
 */

/**
 * Native YAPL function to prompt the user for an input and return the given input as an YAPL Value.
 */
static Value inputNative(int argCount, Value *args) {
    CHECK_ARGS(>, argCount, 1);

    if (argCount == 1) {
        Value prompt = *args;
        if (!IS_STRING(prompt)) { /* Checks if the prompt is valid */
            VMError(ARGS_TYPE_ERR, 1, "string");
            return ERR_VAL;
        }

        printf("%s", AS_CLANG_STRING(prompt)); /* Prints the prompt */
    }

    char *inputString = readStrStdin();                           /* Reads the input string */
    return OBJ_VAL(copyString(inputString, strlen(inputString))); /* Creates the YAPL string */
}

/**
 * Native YAPL function to print an YAPL value.
 */
static Value printNative(int argCount, Value *args) {
    CHECK_ARGS(!=, argCount, 1);
    printValue(*args);
    return NULL_VAL;
}

/**
 * Native YAPL function to print (with a new line) an YAPL value.
 */
static Value printlnNative(int argCount, Value *args) {
    CHECK_ARGS(!=, argCount, 1);
    printValue(*args);
    printf("\n");
    return NULL_VAL;
}

/* End CHECK_ARGS */
#undef CHECK_ARGS

/*
 * ================================================================================================
 * ==================================== Native functions setup ====================================
 * ================================================================================================
 */

/**
 * Defines a new native function for YAPL.
 */
void defineNative(const char *name, NativeFn function) {
    push(OBJ_VAL(copyString(name, (int) strlen(name))));
    push(OBJ_VAL(newNative(function)));
    tableSet(&vm.globals, AS_STRING(vm.stack[0]), vm.stack[1]);
    pop();
    pop();
}

/**
 * Defines the complete set of native function for YAPL.
 */
void defineNatives() {
    const char *nativeNames[] = { /* Native functions names */
        "authors",
        "license",
        "help",
        "exit",
        "clock",
        "time",
        "input",
        "print",
        "println"
    };

    const NativeFn nativeFunctions[] = { /* Native functions C implementations */
        authorsNative,
        licenseNative,
        helpNative,
        exitNative,
        clockNative,
        timeNative,
        inputNative,
        printNative,
        printlnNative
    };

    /* Define listed native functions */
    for (unsigned long i = 0; i < sizeof(nativeNames) / sizeof(nativeNames[0]); i++)
        defineNative(nativeNames[i], nativeFunctions[i]);
}
