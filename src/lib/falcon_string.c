/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * falcon_string.c: Falcon's standard string library
 * See Falcon's license in the LICENSE file
 */

#include "falcon_string.h"
#include "../vm/falcon_memory.h"
#include <string.h>

/**
 * Hashes an input string using FNV-1a hash function.
 */
uint32_t FalconHashString(const unsigned char *key, size_t length) {
    uint32_t hash = 2166136261u;

    for (uint64_t i = 0; i < length; i++) {
        hash ^= key[i];
        hash *= 16777619;
    }

    return hash;
}

/**
 * Creates a new FalconObjString by claiming ownership of the given string. In this case, the
 * characters of a FalconObjString can be freed when no longer needed.
 */
FalconObjString *FalconMakeString(FalconVM *vm, size_t length) {
    FalconObjString *str = (FalconObjString *) FalconAllocateObject(
        vm, sizeof(FalconObjString) + length + 1, FALCON_OBJ_STRING);
    str->length = length;
    return str;
}

/**
 * Copies and allocates a given string to the heap. This way, every FalconObjString reliably owns
 * its character array and can free it.
 */
FalconObjString *FalconCopyString(FalconVM *vm, const char *chars, size_t length) {
    uint32_t hash = FalconHashString((const unsigned char *) chars, length);
    FalconObjString *interned =
        FalconTableFindStr(&vm->strings, chars, length, hash); /* Checks if interned */
    if (interned != NULL) return interned;

    FalconObjString *str = FalconMakeString(vm, length);
    memcpy(str->chars, chars, length);
    str->chars[length] = '\0';
    str->hash = hash;

    FalconPush(vm, FALCON_OBJ_VAL(str)); /* Adds to stack to avoid being garbage collected */
    FalconTableSet(vm, &vm->strings, str, FALCON_NULL_VAL); /* Interns the string */
    FalconPop(vm);
    return str;
}

/**
 * Compares two given Falcon strings. If the two strings are equal, returns 0. If the first string
 * is lexicographically smaller, returns a negative integer. Otherwise, returns a positive one.
 */
int FalconCompareStrings(const FalconObjString *str1, const FalconObjString *str2) {
    const unsigned char *s1 = (const unsigned char *) str1->chars;
    const unsigned char *s2 = (const unsigned char *) str2->chars;
    while ((*s1 && *s2) && (*s1 == *s2)) {
        s1++; s2++;
    }
    return *s1 - *s2;
}

/**
 * Concatenates two given Falcon strings.
 */
FalconObjString *FalconConcatStrings(FalconVM *vm, const FalconObjString *s1,
                                     const FalconObjString *s2) {
    size_t length = s2->length + s1->length;
    FalconObjString *result = FalconMakeString(vm, length);
    memcpy(result->chars, s2->chars, s2->length);
    memcpy(result->chars + s2->length, s1->chars, s1->length);
    result->chars[length] = '\0';
    result->hash = FalconHashString((const unsigned char *) result->chars, length);
    return result;
}
