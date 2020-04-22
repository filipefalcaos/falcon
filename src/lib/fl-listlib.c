/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * fl-listlib.c: Falcon's standard list library
 * See Falcon's license in the LICENSE file
 */

#include "fl-listlib.h"
#include "../core/fl-mem.h"
#include <stdio.h>
#include <string.h>

/**
 * Concatenates two given Falcon lists.
 */
ObjList *concat_lists(FalconVM *vm, const ObjList *list1, const ObjList *list2) {
    int length = list1->elements.count + list2->elements.count;
    ObjList *result = FALCON_ALLOCATE_OBJ(vm, ObjList, OBJ_LIST);
    result->elements.count = length;
    result->elements.capacity = length;
    result->elements.values = FALCON_ALLOCATE(vm, FalconValue, length);

    for (int i = 0; i < list2->elements.count; i++) /* Adds "list2" values */
        result->elements.values[i] = list2->elements.values[i];

    for (int i = 0; i < list1->elements.count; i++) /* Adds "list1" values */
        result->elements.values[i + list2->elements.count] = list1->elements.values[i];

    return result;
}

/**
 * Converts a given Falcon List to a Falcon string.
 */
ObjString *list_to_string(FalconVM *vm, ObjList *list) {
    size_t allocationSize = MIN_COLLECTION_TO_STR;
    char *string = FALCON_ALLOCATE(vm, char, allocationSize); /* Initial allocation */
    int stringLen = sprintf(string, "[");
    int elementsCount = list->elements.count;

    /* Adds the elements to the string */
    for (int i = 0; i < elementsCount; i++) {
        char *elementString;
        size_t elementLen, currLen;
        bool isString;

        /* Gets the current element string */
        if (IS_STRING(list->elements.values[i])) {
            ObjString *objString = AS_STRING(list->elements.values[i]);
            elementString = objString->chars;
            elementLen = objString->length;
            currLen = stringLen + elementLen + 5; /* +5 = quotes (2) + ", " (2) + init space (1) */
            isString = true;
        } else {
            elementString = value_to_string(vm, &list->elements.values[i])->chars;
            elementLen = strlen(elementString);
            currLen = stringLen + elementLen + 3; /* +3 = ", " (2) + init space (1) */
            isString = false;
        }

        /* Increases the allocation, if needed */
        if (currLen > allocationSize) {
            int oldSize = allocationSize;
            allocationSize = increaseStringAllocation(currLen, allocationSize);
            string = FALCON_INCREASE_ARRAY(vm, string, char, oldSize, allocationSize);
        }

        /* Appends the current element to the output string */
        if (isString) {
            stringLen += sprintf(string + stringLen, " \"%s\"", elementString);
        } else {
            stringLen += sprintf(string + stringLen, " %s", elementString);
        }

        /* Appends the separator or final space */
        if (i != (elementsCount - 1)) {
            stringLen += sprintf(string + stringLen, ",");
        } else {
            stringLen += sprintf(string + stringLen, " ");
        }
    }

    sprintf(string + stringLen, "]");
    return new_ObjString(vm, string, strlen(string));
}
