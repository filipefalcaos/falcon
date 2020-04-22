/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * fl-iolib.c: Falcon's standard IO library
 * See Falcon's license in the LICENSE file
 */

#include "fl-iolib.h"
#include "../core/fl-mem.h"
#include <stdlib.h>
#include <string.h>

/**
 * Reads the content of an input file, given its path. If the function fails to open the file or to
 * allocate memory to read its content, an error message will be printed and the process will exit.
 * Otherwise, a string with the file content will be returned.
 */
char *read_file(FalconVM *vm, const char *path) {
    char *buffer;
    FILE *file = fopen(path, "rb"); /* Opens the input file */

    if (file == NULL) { /* Failed to open the file? */
        fprintf(stderr, "%s '%s'.\n", IO_OPEN_FILE_ERR, path);
        exit(FALCON_ERR_OS);
    }

    fseek(file, 0L, SEEK_END);
    size_t fileSize = (size_t) ftell(file);           /* Gets the file size */
    buffer = FALCON_ALLOCATE(vm, char, fileSize + 1); /* Allocates the size + 1, for EOF */
    rewind(file);

    size_t bytesRead =
        fread(buffer, sizeof(char), fileSize, file); /* Reads the file in a single batch */

    if (bytesRead < fileSize) { /* Failed to read the entire file? */
        fprintf(stderr, "%s '%s'.\n", IO_READ_FILE_ERR, path);
        exit(FALCON_ERR_OS);
    }

    buffer[bytesRead] = '\0';
    fclose(file);
    return buffer;
}

/* Initial string allocation size */
#define STR_INITIAL_ALLOC 128

/**
 * Reads an input string from stdin, dynamically allocating memory if needed.
 */
static char *read_str_stdin(FalconVM *vm) {
    uint64_t initialLength = STR_INITIAL_ALLOC;             /* Initial allocation size */
    char *input = FALCON_ALLOCATE(vm, char, initialLength); /* Allocates initial space */
    uint64_t currentSize = initialLength;

    uint64_t i = 0;
    int currentChar = getchar(); /* Reads the first char */

    while (currentChar != '\n' && currentChar != EOF) {
        input[i++] = (char) currentChar;
        if (i == currentSize) {
            currentSize = FALCON_INCREASE_CAPACITY(i);
            input = reallocate(vm, input, i, currentSize); /* Increases string size */
        }

        currentChar = getchar(); /* Reads the next char */
    }

    input[i] = '\0';
    return input;
}

#undef STR_INITIAL_ALLOC

/**
 * Prompts the user for an input and returns the given input as a ObjString. Accepts only one
 * optional argument, a ObjString that represents a prompt (e.g., ">>>").
 * TODO: check if stdin is a tty
 */
FalconValue lib_input(FalconVM *vm, int argCount, FalconValue *args) {
    ASSERT_ARGS_COUNT(vm, >, argCount, 1);
    if (argCount == 1) {
        FalconValue prompt = *args;
        ASSERT_ARG_TYPE(IS_STRING, "string", prompt, vm, 1); /* Checks if is valid */
        printf("%s", AS_CSTRING(prompt));                    /* Prints the prompt */
    }

    char *inputString = read_str_stdin(vm); /* Reads the input string */
    return OBJ_VAL(new_ObjString(vm, inputString, strlen(inputString)));
}
