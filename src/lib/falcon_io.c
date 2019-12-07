/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * falcon_io.c: Falcon's standard IO library
 * See Falcon's license in the LICENSE file
 */

#include "falcon_io.h"
#include "../vm/falcon_memory.h"
#include <stdlib.h>

/**
 * Reads the content of an input file.
 */
char *falconReadFile(FalconVM *vm, const char *path) {
    char *buffer;
    FILE *file = fopen(path, "rb"); /* Opens the input file */

    if (file == NULL) { /* Failed to open the file */
        fprintf(stderr, "%s \"%s\"\n", FALCON_OPEN_ERR, path);
        exit(FALCON_ERR_OS);
    }

    fseek(file, 0L, SEEK_END);
    size_t fileSize = (size_t) ftell(file);           /* Gets the file size */
    buffer = FALCON_ALLOCATE(vm, char, fileSize + 1); /* Allocates the size + 1, for EOF */
    rewind(file);

    size_t bytesRead =
        fread(buffer, sizeof(char), fileSize, file); /* Reads the file in a single batch */

    if (bytesRead < fileSize) { /* Failed to read the entire file? */
        fprintf(stderr, "%s \"%s\"\n", FALCON_READ_ERR, path);
        exit(FALCON_ERR_OS);
    }

    buffer[bytesRead] = '\0';
    fclose(file);
    return buffer;
}

/* Initial string allocation size */
#define STR_INITIAL_ALLOC 128

/**
 * Reads an input string from the standard input dynamically allocating memory.
 */
char *falconReadStrStdin(FalconVM *vm) {
    uint64_t currentSize = 0;
    uint64_t initialLength = STR_INITIAL_ALLOC;             /* Initial allocation size */
    char *input = FALCON_ALLOCATE(vm, char, initialLength); /* Allocates initial space */
    currentSize = initialLength;

    uint64_t i = 0;
    int currentChar = getchar(); /* Reads the first char */

    while (currentChar != '\n' && currentChar != EOF) {
        input[i++] = (char) currentChar;
        if (i == currentSize) {
            currentSize = FALCON_INCREASE_CAPACITY(i);
            input = falconReallocate(vm, input, i, currentSize); /* Increases string size */
        }

        currentChar = getchar(); /* Reads the next char */
    }

    input[i] = '\0';
    return input;
}

#undef STR_INITIAL_ALLOC

/**
 * Prints a string character by character until a specified character is found.
 */
void falconPrintUntil(FILE *file, const char *str, char delimiter) {
    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] == delimiter) break;
        fprintf(file, "%c", str[i]);
    }
}
