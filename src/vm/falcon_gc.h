/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * falcon_gc.h: Falcon's garbage collector
 * See Falcon's license in the LICENSE file
 */

#ifndef FALCON_GC_H
#define FALCON_GC_H

#include "falcon_vm.h"

/* Garbage collector operations */
void FalconRunGC(FalconVM *vm);

#endif // FALCON_GC_H
