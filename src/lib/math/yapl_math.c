/*
 * YAPL version 0.0.1 (master, Oct 18 2019)
 * yapl_math.c: YAPL's standard mathematical library
 * See YAPL's license in the LICENSE file
 */

#include "yapl_math.h"

/**
 * Gets the number of digits in a given integer.
 */
int getDigits(int n) { return (int) floor(log10(n) + 1); }

/**
 * Gets the absolute value of a given double number.
 */
double getAbs(double value) { return fabs(value); }

/**
 * Gets the square root value of a given double number.
 */
double getSqrt(double value) { return sqrt(value); }
