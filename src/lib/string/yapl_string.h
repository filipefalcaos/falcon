/*
 * YAPL version 0.0.1 (master, Oct 18 2019)
 * yapl_string.c: YAPL's standard string library
 * See YAPL's license in the LICENSE file
 */

#ifndef YAPL_STRING_H
#define YAPL_STRING_H

#include "../../vm/yapl_value.h"

/* String constants */
#define STR_INITIAL_ALLOC 128

/* String operations */
ObjString *readStrStdin();

#endif // YAPL_STRING_H
