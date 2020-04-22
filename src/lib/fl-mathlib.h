/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * fl-mathlib.h: Falcon's standard math library
 * See Falcon's license in the LICENSE file
 */

#ifndef FALCON_MATH_H
#define FALCON_MATH_H

#include "../core/fl-vm.h"

/* Calculates and returns the absolute value of a given numeric FalconValue. Only one numerical
 * argument is accepted */
FalconValue lib_abs(FalconVM *vm, int argCount, FalconValue *args);

/* Calculates and returns the square root of a given numeric FalconValue. Only one numerical
 * argument is accepted */
FalconValue lib_sqrt(FalconVM *vm, int argCount, FalconValue *args);

/* Calculates and returns the value of "x" to the power of "y", where "x" and "y" must be two given
 * numeric FalconValues */
FalconValue lib_pow(FalconVM *vm, int argCount, FalconValue *args);

#endif // FALCON_MATH_H
