/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * falcon_string.h: Falcon's standard string library
 * See Falcon's license in the LICENSE file
 */

#ifndef FALCON_STRING_H
#define FALCON_STRING_H

#include "../../commons.h"
#include "../../vm/falcon_value.h"
#include "../../vm/falcon_vm.h"

/* String operations */
uint32_t FalconHashString(const char *key, int length);
FalconObjString *FalconMakeString(VM *vm, int length);
FalconObjString *FalconCopyString(VM *vm, const char *chars, int length);
FalconObjString *FalconConcatStrings(VM *vm, FalconObjString *str1, FalconObjString *str2);

#endif // FALCON_STRING_H
