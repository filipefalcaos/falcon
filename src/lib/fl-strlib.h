/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * fl-strlib.h: Falcon's standard string library
 * See Falcon's license in the LICENSE file
 */

#ifndef FALCON_STRING_H
#define FALCON_STRING_H

#include "../core/fl-value.h"
#include "../core/fl-vm.h"

/* String operations */
uint32_t hash_string(const unsigned char *key, size_t length);
int cmp_strings(const ObjString *str1, const ObjString *str2);
ObjString *concat_strings(FalconVM *vm, const ObjString *str1, const ObjString *str2);

#endif // FALCON_STRING_H
