/*
 * YAPL version 0.0.1 (master, Oct 18 2019)
 * yapl_natives.c: YAPL's native functions
 * See YAPL's license in the LICENSE file
 */

#include "yapl_natives.h"
#include "../vm/yapl_memmanager.h"
#include "yapl_string.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

/*
 * ================================================================================================
 * ================================ System-related native functions ===============================
 * ================================================================================================
 */

/**
 * Native YAPL function to compute the elapsed time since the program started running, in seconds.
 */
static Value clockNative(int argCount, Value *args) {
    if (argCount != 0) {
        VMError(ARGS_COUNT_ERR, 0, argCount);
        return ERR_VAL;
    }

    return NUM_VAL((double) clock() / CLOCKS_PER_SEC);
}

/**
 * Native YAPL function to compute the UNIX timestamp, in seconds.
 */
static Value timeNative(int argCount, Value *args) {
    if (argCount != 0) {
        VMError(ARGS_COUNT_ERR, 0, argCount);
        return ERR_VAL;
    }

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
    if (argCount > 1) {
        VMError(ARGS_COUNT_ERR, 1, argCount);
        return ERR_VAL;
    }

    if (argCount == 1) {
        Value prompt = args[0];
        if (!IS_STRING(prompt)) { /* Checks if the prompt is valid */
            VMError(ARGS_TYPE_ERR, 1, "string");
            return ERR_VAL;
        }

        printf("%s", AS_CLANG_STRING(prompt)); /* Prints the prompt */
    }

    return OBJ_VAL(readStrStdin()); /* Creates the YAPL string */
}

/**
 * Native YAPL function to print an YAPL value.
 */
static Value printNative(int argCount, Value *args) {
    if (argCount != 1) {
        VMError(ARGS_COUNT_ERR, 1, argCount);
        return ERR_VAL;
    }

    printValue(*args);
    return NULL_VAL;
}

/**
 * Native YAPL function to print (with a new line) an YAPL value.
 */
static Value printlnNative(int argCount, Value *args) {
    if (argCount != 1) {
        VMError(ARGS_COUNT_ERR, 1, argCount);
        return ERR_VAL;
    }

    printValue(*args);
    printf("\n");
    return NULL_VAL;
}

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
        "clock",
        "time",
        "input",
        "print",
        "println"
    };

    const NativeFn nativeFunctions[] = { /* Native functions C implementations */
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
