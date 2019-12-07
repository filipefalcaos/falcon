/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * falcon_math.h: Falcon's standard mathematical library
 * See Falcon's license in the LICENSE file
 */

#ifndef FALCON_MATH_H
#define FALCON_MATH_H

#include <math.h>

/* Mathematical operations */
int falconGetDigits(int n);
double falconAbs(double value);
double falconSqrt(double value);
double falconPow(double x, double y);

#endif // FALCON_MATH_H
