/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * falcon_natives.c: Falcon's native functions
 * See Falcon's license in the LICENSE file
 */

#ifndef FALCON_NATIVES_H
#define FALCON_NATIVES_H

#include "../vm/falcon_vm.h"
#include "falcon_baselib.h"
#include "falcon_io.h"
#include "falcon_math.h"
#include "falcon_sys.h"

/* Native functions operations */
void defineNatives(FalconVM *vm);

#endif // FALCON_NATIVES_H
