/*
 * YAPL version 0.0.1 (master, Oct 18 2019)
 * yapl_string.c: YAPL's standard string library
 * See YAPL's license in the LICENSE file
 */

#include "yapl_string.h"
#include "../../vm/yapl_object.h"
#include "../../vm/yapl_memmanager.h"
#include <string.h>

/**
 * Hashes an input string using FNV-1a hash function.
 */
uint32_t hashString(const char *key, int length) {
    uint32_t hash = 2166136261u;

    for (uint64_t i = 0; i < length; i++) {
        hash ^= key[i];
        hash *= 16777619;
    }

    return hash;
}

/**
 * Creates a new ObjString by claiming ownership of the given string. In this case, the characters
 * of a ObjString can be freed when no longer needed.
 */
ObjString *makeString(VM *vm, int length) {
    ObjString *str = (ObjString *) allocateObject(vm, sizeof(ObjString) + length + 1, OBJ_STRING);
    str->length = length;
    return str;
}

/**
 * Copies and allocates a given string to the heap. This way, every ObjString reliably owns its
 * character array and can free it.
 */
ObjString *copyString(VM *vm, const char *chars, int length) {
    uint32_t hash = hashString(chars, length);
    ObjString *interned = tableFindStr(&vm->strings, chars, length, hash); /* Checks if interned */
    if (interned != NULL) return interned;

    ObjString *str = makeString(vm, length);
    memcpy(str->chars, chars, length);
    str->chars[length] = '\0';
    str->hash = hash;

    tableSet(&vm->strings, str, NULL_VAL); /* Intern the string */
    return str;
}

/**
 * Concatenates two given YAPL strings.
 */
ObjString *concatStrings(VM *vm, ObjString *str1, ObjString *str2) {
    int length = str2->length + str1->length;
    ObjString *result = makeString(vm, length);
    memcpy(result->chars, str2->chars, str2->length);
    memcpy(result->chars + str2->length, str1->chars, str1->length);
    result->chars[length] = '\0';
    result->hash = hashString(result->chars, length);
    return result;
}
