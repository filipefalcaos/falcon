/*
 * YAPL version 0.0.1 (master, Oct 18 2019)
 * yapl_natives.h: YAPL's native functions
 * See YAPL's license in the LICENSE file
 */

#ifndef YAPL_NATIVES_H
#define YAPL_NATIVES_H

#include "../vm/yapl_vm.h"

/* Native functions operations */
void defineNative(const char *name, NativeFn function);
void defineNatives();

/* Native functions errors */
#define CONV_STR_NUM_ERR "Could not convert string to number."

#endif // YAPL_NATIVES_H
