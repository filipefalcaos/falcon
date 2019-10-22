/*
 * YAPL version 0.0.1 (master, Oct 18 2019)
 * yapl_utils.c: List of internal utility functions
 * See YAPL's license in the LICENSE file
 */

#include "yapl_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* File errors */
#define ERROR_OPEN "Could not open file"
#define ERROR_READ "Could not read file"

/**
 * Checks if two strings are equal.
 */
bool areStrEqual(const char *str1, const char *str2) { return (strcmp(str1, str2) == 0); }

/**
 * Checks if two characters are equal.
 */
bool areCharEqual(char chr1, char chr2) { return (chr1 == chr2); }

/**
 * Reads the content of an input file.
 */
char *readFile(const char *path) {
    char *buffer;
    FILE *file = fopen(path, "rb"); /* Opens the input file */

    if (file == NULL) { /* Failed to open the file */
        fprintf(stderr, "%s \"%s\"\n", ERROR_OPEN, path);
        exit(74);
    }

    fseek(file, 0L, SEEK_END);
    size_t fileSize = (size_t) ftell(file); /* Gets the file size */
    buffer = (char *) malloc(fileSize + 1); /* Allocates the size + 1, for EOF */
    rewind(file);

    if (buffer == NULL) { /* Failed to allocate the source string */
        fprintf(stderr, "%s \"%s\"\n", ERROR_READ, path);
        exit(74);
    }

    size_t bytesRead =
        fread(buffer, sizeof(char), fileSize, file); /* Reads the file in a single batch */
    if (bytesRead < fileSize) {                      /* Failed to read the entire file */
        fprintf(stderr, "%s \"%s\"\n", ERROR_READ, path);
        exit(74);
    }

    buffer[bytesRead] = '\0';
    fclose(file);
    return buffer;
}
