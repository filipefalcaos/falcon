/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * falcon_math.c: Falcon's standard mathematical library
 * See Falcon's license in the LICENSE file
 */

#include "falcon_math.h"

/**
 * Gets the number of digits in a given integer.
 */
int FalconGetDigits(int n) { return (int) floor(log10(n) + 1); }

/**
 * Gets the absolute value of a given double number.
 */
double FalconAbs(double value) { return fabs(value); }

/**
 * Gets the square root value of a given double number.
 */
double FalconSqrt(double value) { return sqrt(value); }

/**
 * Gets the value of a number "x" to the power of a number "y".
 */
double FalconPow(double x, double y) { return pow(x, y); }
