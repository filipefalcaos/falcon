/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * fl-gc.h: Falcon's garbage collector
 * See Falcon's license in the LICENSE file
 */

#ifndef FALCON_GC_H
#define FALCON_GC_H

#include "fl-vm.h"

/* Enables/disables the garbage collector */
#define ENABLE_GC(vm)  vm->gcEnabled = true
#define DISABLE_GC(vm) vm->gcEnabled = false

/* Starts a new garbage collector run */
void run_GC(FalconVM *vm);

#endif // FALCON_GC_H
