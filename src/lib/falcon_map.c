/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * falcon_map.h: Falcon's standard map library
 * See Falcon's license in the LICENSE file
 */

#include "falcon_map.h"
#include "../vm/falcon_memory.h"
#include <stdio.h>
#include <string.h>

/**
 * Converts a given Falcon Map to a Falcon string.
 */
ObjString *mapToString(FalconVM *vm, ObjMap *map) {
    size_t allocationSize = MIN_COLLECTION_TO_STR;
    char *string = FALCON_ALLOCATE(vm, char, allocationSize); /* Initial allocation */
    int stringLen = sprintf(string, "{");
    int pairsCount = map->entries.count;

    uint16_t currCount = 0;
    Entry *currEntry = map->entries.entries;

    /* Adds the pairs to the string */
    while (true) {
        char *keyString, *valueString;
        size_t keyLen, valueLen, currLen;
        bool isString;

        if (currCount == pairsCount) /* All elements found? */
            break;

        if (currEntry->key != NULL) {
            keyString = currEntry->key->chars;
            keyLen = currEntry->key->length;
            currLen = stringLen + keyLen + 5; /* +5 = quotes (2) + ": " (2) + initial space (1) */

            /* Increases the allocation, if needed */
            if (currLen > allocationSize) {
                int oldSize = allocationSize;
                allocationSize = increaseStringAllocation(currLen, allocationSize);
                string = FALCON_INCREASE_ARRAY(vm, string, char, oldSize, allocationSize);
            }

            /* Appends the current element to the output string */
            stringLen += sprintf(string + stringLen, " \"%s\": ", keyString);

            /* Gets the current value string */
            if (IS_STRING(currEntry->value)) {
                ObjString *objString = AS_STRING(currEntry->value);
                valueString = objString->chars;
                valueLen = objString->length;
                currLen = stringLen + valueLen + 4; /* +4 = quotes (2) + ", " (2) */
                isString = true;
            } else {
                valueString = valueToString(vm, &currEntry->value)->chars;
                valueLen = strlen(valueString);
                currLen = stringLen + valueLen + 2; /* +2 for the ", " */
                isString = false;
            }

            /* Increases the allocation, if needed */
            if (currLen > allocationSize) {
                int oldSize = allocationSize;
                allocationSize = increaseStringAllocation(currLen, allocationSize);
                string = FALCON_INCREASE_ARRAY(vm, string, char, oldSize, allocationSize);
            }

            /* Appends the current value to the output string */
            if (isString) {
                stringLen += sprintf(string + stringLen, "\"%s\"", valueString);
            } else {
                stringLen += sprintf(string + stringLen, "%s", valueString);
            }

            /* Appends the separator or final space */
            if (currCount != pairsCount - 1) {
                stringLen += sprintf(string + stringLen, ",");
            } else {
                stringLen += sprintf(string + stringLen, " ");
            }

            currCount++;
        }

        currEntry++; /* Goes to the next slot */
    }

    sprintf(string + stringLen, "}");
    return falconString(vm, string, strlen(string));
}
