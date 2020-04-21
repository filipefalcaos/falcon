/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * falcon_gc.h: Falcon's garbage collector
 * See Falcon's license in the LICENSE file
 */

#ifndef FALCON_GC_H
#define FALCON_GC_H

#include "falcon_vm.h"

/* Enables/disables the garbage collector */
#define ENABLE_GC(vm)  vm->gcEnabled = true
#define DISABLE_GC(vm) vm->gcEnabled = false

/* Starts a new garbage collector run */
void falconRunGC(FalconVM *vm);

#endif // FALCON_GC_H
