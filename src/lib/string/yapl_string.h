/*
 * YAPL version 0.0.1 (master, Oct 18 2019)
 * yapl_string.c: YAPL's standard string library
 * See YAPL's license in the LICENSE file
 */

#ifndef YAPL_STRING_H
#define YAPL_STRING_H

#include "../../commons.h"
#include "../../vm/yapl_value.h"
#include "../../vm/yapl_vm.h"

/* String operations */
uint32_t hashString(const char *key, int length);
ObjString *makeString(VM *vm, int length);
ObjString *copyString(VM *vm, const char *chars, int length);
ObjString *concatStrings(VM *vm, ObjString *str1, ObjString *str2);

#endif // YAPL_STRING_H
