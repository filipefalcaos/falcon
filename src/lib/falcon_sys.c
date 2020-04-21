/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * falcon_sys.h: Falcon's standard operating system library
 * See Falcon's license in the LICENSE file
 */

#include "falcon_sys.h"
#include <stdlib.h>
#include <time.h>

/**
 * Exits the running process with a given exit code. Only one numerical is accepted. If the
 * argument is a double number, it will be casted to C's int. No return value is provided.
 */
FalconValue lib_exit(FalconVM *vm, int argCount, FalconValue *args) {
    ASSERT_ARGS_COUNT(vm, !=, argCount, 1);
    ASSERT_ARG_TYPE(IS_NUM, "number", *args, vm, 1);
    exit((int) AS_NUM(*args)); /* Exits the process */
}

/**
 * Computes and returns the elapsed time since the current process started running, in seconds. No
 * arguments are accepted.
 */
FalconValue lib_clock(FalconVM *vm, int argCount, FalconValue *args) {
    (void) args; /* Unused */
    ASSERT_ARGS_COUNT(vm, !=, argCount, 0);
    return NUM_VAL((double) clock() / CLOCKS_PER_SEC);
}

/**
 * Computes and returns the UNIX timestamp, in seconds. No arguments are accepted.
 */
FalconValue lib_time(FalconVM *vm, int argCount, FalconValue *args) {
    (void) args; /* Unused */
    ASSERT_ARGS_COUNT(vm, !=, argCount, 0);
    return NUM_VAL((double) time(NULL));
}
