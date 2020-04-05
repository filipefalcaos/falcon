/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * falcon_error.h: Falcon's Compiler/VM error treatment
 * See Falcon's license in the LICENSE file
 */

#ifndef FALCON_ERROR_H
#define FALCON_ERROR_H

#include "../compiler/falcon_compiler.h"
#include <stdarg.h>

/* Compiler/runtime error handling functions */
void compilerError(FalconCompiler *compiler, Token *token, const char *message);
void interpreterError(FalconVM *vm, const char *format, ...);

#endif // FALCON_ERROR_H
