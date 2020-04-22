/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * fl-strlib.c: Falcon's standard string library
 * See Falcon's license in the LICENSE file
 */

#include "fl-strlib.h"
#include <string.h>

/**
 * Hashes an input string using FNV-1a hash function.
 */
uint32_t hash_string(const unsigned char *key, size_t length) {
    uint32_t hash = 2166136261u;

    for (uint64_t i = 0; i < length; i++) {
        hash ^= key[i];
        hash *= 16777619;
    }

    return hash;
}

/**
 * Compares two given Falcon strings. If the two strings are equal, returns 0. If the first string
 * is lexicographically smaller, returns a negative integer. Otherwise, returns a positive one.
 */
int cmp_strings(const ObjString *str1, const ObjString *str2) {
    const unsigned char *s1 = (const unsigned char *) str1->chars;
    const unsigned char *s2 = (const unsigned char *) str2->chars;

    while ((*s1 && *s2) && (*s1 == *s2)) {
        s1++;
        s2++;
    }

    return *s1 - *s2;
}

/**
 * Concatenates two given Falcon strings.
 */
ObjString *concat_strings(FalconVM *vm, const ObjString *str1, const ObjString *str2) {
    size_t length = str2->length + str1->length;
    ObjString *result = make_string(vm, length);
    memcpy(result->chars, str2->chars, str2->length);
    memcpy(result->chars + str2->length, str1->chars, str1->length);
    result->chars[length] = '\0';
    result->hash = hash_string((const unsigned char *) result->chars, length);
    return result;
}
