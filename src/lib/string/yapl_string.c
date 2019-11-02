/*
 * YAPL version 0.0.1 (master, Oct 18 2019)
 * yapl_string.c: YAPL's standard string library
 * See YAPL's license in the LICENSE file
 */

#include "yapl_string.h"
#include "../../vm/yapl_object.h"
#include "../../vm/yapl_vm.h"
#include <string.h>

/**
 * Hashes an input string using FNV-1a hash function.
 */
uint32_t hashString(const char *key, int length) {
    uint32_t hash = 2166136261u;

    for (int i = 0; i < length; i++) {
        hash ^= key[i];
        hash *= 16777619;
    }

    return hash;
}

/**
 * Creates a new ObjString by claiming ownership of the given string. In this case, the characters
 * of a ObjString can be freed when no longer needed.
 */
ObjString *makeString(int length) {
    ObjString *string = (ObjString *) allocateObject(sizeof(ObjString) + length + 1, OBJ_STRING);
    string->length = length;
    return string;
}

/**
 * Copies and allocates a given string to the heap. This way, every ObjString reliably owns its
 * character array and can free it.
 */
ObjString *copyString(const char *chars, int length) {
    uint32_t hash = hashString(chars, length);
    ObjString *interned = tableFindStr(&vm.strings, chars, length, hash); /* Checks if interned */
    if (interned != NULL) return interned;

    ObjString *string = makeString(length);
    memcpy(string->chars, chars, length);
    string->chars[length] = '\0';
    string->hash = hash;
    tableSet(&vm.strings, string, NULL_VAL); /* Intern the string */
    return string;
}

/**
 * Concatenates two given YAPL strings.
 */
ObjString *concatStrings(ObjString *str1, ObjString *str2) {
    int length = str2->length + str1->length;
    ObjString *result = makeString(length);
    memcpy(result->chars, str2->chars, str2->length);
    memcpy(result->chars + str2->length, str1->chars, str1->length);
    result->chars[length] = '\0';
    result->hash = hashString(result->chars, length);
    return result;
}
