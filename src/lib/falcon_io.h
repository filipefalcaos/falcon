/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * falcon_io.c: Falcon's standard IO library
 * See Falcon's license in the LICENSE file
 */

#ifndef FALCON_IO_H
#define FALCON_IO_H

#include "../vm/falcon_vm.h"
#include <stdio.h>

/* File operations */
char *readFile(FalconVM *vm, const char *path);
char *readStrStdin(FalconVM *vm);
void printUntil(FILE *file, const char *str, char delimiter);

/* Readline errors */
#define IO_READLINE_ERR "Could not read input line."

/* File errors */
#define IO_OPEN_FILE_ERR "Could not open file."
#define IO_READ_FILE_ERR "Could not read file."

#endif // FALCON_IO_H
