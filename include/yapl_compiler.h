/*
 * YAPL version 0.0.1 (master, Oct 18 2019)
 * yapl_compiler.h: YAPL's handwritten parser/compiler based on Pratt Parsing
 * See YAPL's license in the LICENSE file
 */

#ifndef YAPL_COMPILER_H
#define YAPL_COMPILER_H

#include "yapl_vm.h"

/* Compiler operations */
ObjFunction *compile(const char *source);

#endif // YAPL_COMPILER_H
