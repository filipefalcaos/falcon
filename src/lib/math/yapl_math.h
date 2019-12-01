/*
 * YAPL version 0.0.1 (master, Oct 18 2019)
 * yapl_math.h: YAPL's standard mathematical library
 * See YAPL's license in the LICENSE file
 */

#ifndef YAPL_MATH_H
#define YAPL_MATH_H

#include <math.h>

/* Mathematical operations */
int getDigits(int n);
double getAbs(double value);

/* Computes a^b */
#define YAPL_POW(a, b) pow(a, b)

#endif // YAPL_MATH_H
