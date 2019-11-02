/*
 * YAPL version 0.0.1 (master, Oct 18 2019)
 * yapl_io.h: YAPL's standard IO library
 * See YAPL's license in the LICENSE file
 */

#ifndef YAPL_IO_H
#define YAPL_IO_H

#include <stdio.h>

/* File operations */
char *readFile(const char *path);
char *readStrStdin();
void printUntil(FILE *file, const char *str, char delimiter);

/* Allocation constants */
#define STR_INITIAL_ALLOC 128

/* File errors */
#define ERROR_OPEN "Could not open file"
#define ERROR_READ "Could not read file"

#endif // YAPL_IO_H
