/*
 * YAPL version 0.0.1 (master, Oct 18 2019)
 * yapl_utils.h: List of internal utility functions
 * See YAPL's license in the LICENSE file
 */

#ifndef YAPL_UTILS_H
#define YAPL_UTILS_H

#include "commons.h"

/* String/Char utilities */
bool areStrEqual(const char *str1, const char *str2);
bool areCharEqual(char chr1, char chr2);

/* File utilities */
char *readFile(const char *path);

#endif // YAPL_UTILS_H
