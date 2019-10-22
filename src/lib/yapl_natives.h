/*
 * YAPL version 0.0.1 (master, Oct 18 2019)
 * yapl_natives.h: YAPL's native functions
 * See YAPL's license in the LICENSE file
 */

#ifndef YAPL_NATIVES_H
#define YAPL_NATIVES_H

#include "../vm/yapl_value.h"

/* Native functions */
Value clockNative(int argCount, Value *args);
Value printNative(int argCount, Value *args);
Value putsNative(int argCount, Value *args);

#endif // YAPL_NATIVES_H
