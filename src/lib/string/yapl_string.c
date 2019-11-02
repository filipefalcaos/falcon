/*
 * YAPL version 0.0.1 (master, Oct 18 2019)
 * yapl_string.c: YAPL's standard string library
 * See YAPL's license in the LICENSE file
 */

#include "yapl_string.h"
#include "../../vm/yapl_memmanager.h"
#include <stdio.h>
#include <string.h>

/**
 * Reads an input string from the standard input dynamically allocating memory.
 */
ObjString *readStrStdin() {
    uint64_t currentLength = 0;
    uint64_t initialLength = STR_INITIAL_ALLOC;        /* Initial allocation length */
    char *inputString = ALLOCATE(char, initialLength); /* Allocates space for the input string */
    currentLength = initialLength;

    if (inputString == NULL) { /* Checks if the allocation failed */
        memoryError();
        return NULL;
    }

    uint64_t i = 0;
    int currentChar = getchar(); /* Reads the first char */

    while (currentChar != '\n' && currentChar != EOF) {
        inputString[i++] = (char) currentChar;
        if (i == currentLength) {
            currentLength = INCREASE_CAPACITY(i);
            inputString = reallocate(inputString, i, currentLength); /* Increases string size */
        }

        currentChar = getchar(); /* Reads the next char */
    }

    inputString[i] = '\0';
    return copyString(inputString, (int) strlen(inputString)); /* Creates the YAPL string */
}
