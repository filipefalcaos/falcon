/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * falcon_natives.c: Falcon's native functions
 * See Falcon's license in the LICENSE file
 */

#ifndef FALCON_NATIVES_H
#define FALCON_NATIVES_H

#include "../vm/falcon_vm.h"

/* Native functions operations */
void falconDefNatives(FalconVM *vm);

/* Native functions errors */
#define FALCON_CONV_STR_NUM_ERR "Could not convert string to number."

#endif // FALCON_NATIVES_H
