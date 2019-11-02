/*
 * YAPL version 0.0.1 (master, Oct 18 2019)
 * yapl_natives.h: YAPL's native functions
 * See YAPL's license in the LICENSE file
 */

#ifndef YAPL_NATIVES_H
#define YAPL_NATIVES_H

#include "../vm/yapl_vm.h"

/* Native functions implementations */
typedef Value (*NativeFn)(VM *vm, int argCount, Value *args);

/* YAPL's native functions object */
typedef struct {
    Obj obj;
    NativeFn function;
} ObjNative;

/* Native functions operations */
ObjNative *newNative(VM *vm, NativeFn function);
void defineNative(VM *vm, const char *name, NativeFn function);
void defineNatives(VM *vm);

/* Native functions errors */
#define CONV_STR_NUM_ERR "Could not convert string to number."

#endif // YAPL_NATIVES_H
