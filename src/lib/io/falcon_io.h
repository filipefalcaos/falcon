/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * falcon_io.c: Falcon's standard IO library
 * See Falcon's license in the LICENSE file
 */

#ifndef FALCON_IO_H
#define FALCON_IO_H

#include <stdio.h>

/* File operations */
char *FalconReadFile(const char *path);
char *FalconReadStrStdin();
void FalconPrintUntil(FILE *file, const char *str, char delimiter);

/* Allocation constants */
#define FALCON_STR_INITIAL_ALLOC 128

/* File errors */
#define FALCON_OPEN_ERR "Could not open file"
#define FALCON_READ_ERR "Could not read file"

#endif // FALCON_IO_H
