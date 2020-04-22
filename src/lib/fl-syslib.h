/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * fl-syslib.h: Falcon's standard operating system library
 * See Falcon's license in the LICENSE file
 */

#ifndef FALCON_SYS_H
#define FALCON_SYS_H

#include "../core/fl-vm.h"

/* Exits the running process with a given exit code. Only one numerical is accepted. No return
 * value is provided */
FalconValue lib_exit(FalconVM *vm, int argCount, FalconValue *args);

/* Computes and returns the elapsed time since the current process started running, in seconds. No
 * arguments are accepted */
FalconValue lib_clock(FalconVM *vm, int argCount, FalconValue *args);

/* Computes and returns the UNIX timestamp, in seconds. No arguments are accepted */
FalconValue lib_time(FalconVM *vm, int argCount, FalconValue *args);

#endif // FALCON_SYS_H
