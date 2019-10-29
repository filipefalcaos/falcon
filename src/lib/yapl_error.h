/*
 * YAPL version 0.0.1 (master, Oct 18 2019)
 * yapl_error.h: YAPL's compiler/vm error treatment
 * See YAPL's license in the LICENSE file
 */

#ifndef YAPL_ERROR_H
#define YAPL_ERROR_H

#include "../compiler/yapl_compiler.h"
#include <stdarg.h>

/* Compiler/runtime error handling functions */
void compileTimeError(Token *token, const char *message);
void runtimeError(const char *format, va_list args);

#endif // YAPL_ERROR_H
