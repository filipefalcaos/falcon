/*
 * YAPL version 0.0.1 (master, Oct 18 2019)
 * yapl_natives.h: YAPL's native functions
 * See YAPL's license in the LICENSE file
 */

#include "yapl_natives.h"
#include <time.h>

/**
 * Native YAPL function to compute the elapsed time since the program started running, in seconds.
 */
Value clockNative(int argCount, Value *args) {
    return NUMBER_VAL((double) clock() / CLOCKS_PER_SEC);
}

/**
 * Native YAPL function to print an YAPL value.
 */
Value printNative(int argCount, Value *args) {
    printValue(*args, false);
    return NULL_VAL;
}

/**
 * Native YAPL function to print (with a new line) an YAPL value.
 */
Value putsNative(int argCount, Value *args) {
    printValue(*args, true);
    return NULL_VAL;
}
