/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * falcon_math.c: Falcon's standard math library
 * See Falcon's license in the LICENSE file
 */

#include "falcon_math.h"
#include <math.h>

/**
 * Calculates and returns the absolute value of a given numeric FalconValue. Only one numerical
 * argument is accepted. The "fabs" function from "math.h" is used to perform the calculation.
 */
FalconValue lib_abs(FalconVM *vm, int argCount, FalconValue *args) {
    ASSERT_ARGS_COUNT(vm, !=, argCount, 1);
    ASSERT_ARG_TYPE(IS_NUM, "number", *args, vm, 1);
    double absValue = fabs(AS_NUM(*args)); /* Gets the abs value */
    return NUM_VAL(absValue);
}

/**
 * Calculates and returns the square root of a given numeric FalconValue. Only one numerical
 * argument is accepted. The "sqrt" function from "math.h" is used to perform the calculation.
 */
FalconValue lib_sqrt(FalconVM *vm, int argCount, FalconValue *args) {
    ASSERT_ARGS_COUNT(vm, !=, argCount, 1);
    ASSERT_ARG_TYPE(IS_NUM, "number", *args, vm, 1);
    double sqrtValue = sqrt(AS_NUM(*args)); /* Gets the sqrt value */
    return NUM_VAL(sqrtValue);
}

/**
 * Calculates and returns the value of "x" to the power of "y", where "x" and "y" must be two given
 * numeric FalconValues. The "pow" function from "math.h" is used to perform the calculation.
 */
FalconValue lib_pow(FalconVM *vm, int argCount, FalconValue *args) {
    ASSERT_ARGS_COUNT(vm, !=, argCount, 2);
    ASSERT_ARG_TYPE(IS_NUM, "number", args[0], vm, 1);
    ASSERT_ARG_TYPE(IS_NUM, "number", args[1], vm, 2);

    double powValue = pow(AS_NUM(args[0]), AS_NUM(args[1])); /* Gets the pow value */
    return NUM_VAL(powValue);
}
