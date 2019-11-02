/*
 * YAPL version 0.0.1 (master, Oct 18 2019)
 * yapl_string.c: YAPL's standard string library
 * See YAPL's license in the LICENSE file
 */

#ifndef YAPL_STRING_H
#define YAPL_STRING_H

#include "../../commons.h"
#include "../../vm/yapl_value.h"

/* String operations */
uint32_t hashString(const char *key, uint64_t length);
ObjString *makeString(int length);
ObjString *copyString(const char *chars, uint64_t length);
ObjString *concatStrings(ObjString *str1, ObjString *str2);

#endif // YAPL_STRING_H
