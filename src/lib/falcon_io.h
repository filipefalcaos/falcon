/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * falcon_io.c: Falcon's standard IO library
 * See Falcon's license in the LICENSE file
 */

#ifndef FALCON_IO_H
#define FALCON_IO_H

#include "../vm/falcon_vm.h"
#include <stdio.h>

/* Reads the content of an input file, given its path. If the function fails to read the file, an
 * error message will be printed and the process will exit. Otherwise, a string with the file
 * content will be returned */
char *readFile(FalconVM *vm, const char *path);

/* Prompts the user for an input and returns the given input as a ObjString. Accepts only one
 * optional argument, a ObjString that represents a prompt (e.g., ">>>") */
FalconValue lib_input(FalconVM *vm, int argCount, FalconValue *args);

/* Prints to stdout (with a new line at the end) a given list of FalconValues. Any type of value in
 * Falcon can be printed and any number of arguments are accepted */
FalconValue lib_print(FalconVM *vm, int argCount, FalconValue *args);

/* Readline errors */
#define IO_READLINE_ERR "Could not read input line."

/* File errors */
#define IO_OPEN_FILE_ERR "Could not open file"
#define IO_READ_FILE_ERR "Could not read file"

#endif // FALCON_IO_H
