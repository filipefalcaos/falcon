/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * falcon_string.c: Falcon's standard string library
 * See Falcon's license in the LICENSE file
 */

#include "falcon_string.h"
#include "../../vm/falcon_memory.h"
#include <string.h>

/**
 * Hashes an input string using FNV-1a hash function.
 */
uint32_t FalconHashString(const char *key, int length) {
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
FalconObjString *FalconMakeString(VM *vm, int length) {
    FalconObjString *str = (FalconObjString *) FalconAllocateObject(
        vm, sizeof(FalconObjString) + length + 1, OBJ_STRING);
    str->length = length;
    return str;
}

/**
 * Copies and allocates a given string to the heap. This way, every FalconObjString reliably owns
 * its character array and can free it.
 */
FalconObjString *FalconCopyString(VM *vm, const char *chars, int length) {
    uint32_t hash = FalconHashString(chars, length);
    FalconObjString *interned =
        FalconTableFindStr(&vm->strings, chars, length, hash); /* Checks if interned */
    if (interned != NULL) return interned;

    FalconObjString *str = FalconMakeString(vm, length);
    memcpy(str->chars, chars, length);
    str->chars[length] = '\0';
    str->hash = hash;

    FalconTableSet(&vm->strings, str, FALCON_NULL_VAL); /* Intern the string */
    return str;
}

/**
 * Concatenates two given Falcon strings.
 */
FalconObjString *FalconConcatStrings(VM *vm, FalconObjString *str1, FalconObjString *str2) {
    int length = str2->length + str1->length;
    FalconObjString *result = FalconMakeString(vm, length);
    memcpy(result->chars, str2->chars, str2->length);
    memcpy(result->chars + str2->length, str1->chars, str1->length);
    result->chars[length] = '\0';
    result->hash = FalconHashString(result->chars, length);
    return result;
}
