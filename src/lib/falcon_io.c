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
char *FalconReadFile(const char *path) {
    char *buffer;
    FILE *file = fopen(path, "rb"); /* Opens the input file */

    if (file == NULL) { /* Failed to open the file */
        fprintf(stderr, "%s \"%s\"\n", FALCON_OPEN_ERR, path);
        exit(74);
    }

    fseek(file, 0L, SEEK_END);
    size_t fileSize = (size_t) ftell(file); /* Gets the file size */
    buffer = (char *) malloc(fileSize + 1); /* Allocates the size + 1, for EOF */
    rewind(file);

    if (buffer == NULL) { /* Failed to allocate the source string */
        fprintf(stderr, "%s \"%s\"\n", FALCON_READ_ERR, path);
        exit(74);
    }

    size_t bytesRead =
        fread(buffer, sizeof(char), fileSize, file); /* Reads the file in a single batch */
    if (bytesRead < fileSize) {                      /* Failed to read the entire file */
        fprintf(stderr, "%s \"%s\"\n", FALCON_READ_ERR, path);
        exit(74);
    }

    buffer[bytesRead] = '\0';
    fclose(file);
    return buffer;
}

/**
 * Reads an input string from the standard input dynamically allocating memory.
 */
char *FalconReadStrStdin(FalconVM *vm) {
    uint64_t currentLength = 0;
    uint64_t initialLength = FALCON_STR_INITIAL_ALLOC; /* Initial allocation length */
    char *inputString =
        FALCON_ALLOCATE(vm, char, initialLength); /* Allocates space for the input string */
    currentLength = initialLength;

    if (inputString == NULL) { /* Checks if the allocation failed */
        FalconMemoryError();
        return NULL;
    }

    uint64_t i = 0;
    int currentChar = getchar(); /* Reads the first char */

    while (currentChar != '\n' && currentChar != EOF) {
        inputString[i++] = (char) currentChar;
        if (i == currentLength) {
            currentLength = FALCON_INCREASE_CAPACITY(i);
            inputString =
                FalconReallocate(vm, inputString, i, currentLength); /* Increases string size */
        }

        currentChar = getchar(); /* Reads the next char */
    }

    inputString[i] = '\0';
    return inputString;
}

/**
 * Prints a string character by character until a specified character is found.
 */
void FalconPrintUntil(FILE *file, const char *str, char delimiter) {
    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] == delimiter) break;
        fprintf(file, "%c", str[i]);
    }
}
