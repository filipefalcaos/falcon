/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * falcon_string.h: Falcon's standard string library
 * See Falcon's license in the LICENSE file
 */

#ifndef FALCON_STRING_H
#define FALCON_STRING_H

#include "../commons.h"
#include "../vm/falcon_value.h"
#include "../vm/falcon_vm.h"

/* String operations */
ObjString *falconMakeString(FalconVM *vm, size_t length);
ObjString *falconCopyString(FalconVM *vm, const char *chars, size_t length);
int falconCompareStrings(const ObjString *str1, const ObjString *str2);
ObjString *falconConcatStrings(FalconVM *vm, const ObjString *s1, const ObjString *s2);

#endif // FALCON_STRING_H
