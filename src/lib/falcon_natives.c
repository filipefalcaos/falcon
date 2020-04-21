/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * falcon_natives.c: Falcon's native functions
 * See Falcon's license in the LICENSE file
 */

#include "falcon_natives.h"
#include "../vm/falcon_memory.h"
#include <string.h>

/* Native functions implementations */
const ObjNative nativeFunctions[] = {
    {.function = lib_exit, .name = "exit"},         {.function = lib_clock, .name = "clock"},
    {.function = lib_time, .name = "time"},         {.function = lib_type, .name = "type"},
    {.function = lib_bool, .name = "bool"},         {.function = lib_num, .name = "num"},
    {.function = lib_str, .name = "str"},           {.function = lib_len, .name = "len"},
    {.function = lib_hasField, .name = "hasField"}, {.function = lib_getField, .name = "getField"},
    {.function = lib_setField, .name = "setField"}, {.function = lib_delField, .name = "delField"},
    {.function = lib_abs, .name = "abs"},           {.function = lib_sqrt, .name = "sqrt"},
    {.function = lib_pow, .name = "pow"},           {.function = lib_input, .name = "input"},
    {.function = lib_print, .name = "print"}};

/**
 * Defines a new native function for Falcon.
 */
static void defNative(FalconVM *vm, const char *name, FalconNativeFn function) {
    DISABLE_GC(vm); /* Avoids GC from the "defNative" ahead */
    ObjString *strName = falconString(vm, name, (int) strlen(name));
    ENABLE_GC(vm);
    DISABLE_GC(vm); /* Avoids GC from the "defNative" ahead */
    ObjNative *nativeFn = falconNative(vm, function, name);
    mapSet(vm, &vm->globals, strName, OBJ_VAL(nativeFn));
    ENABLE_GC(vm);
}

/**
 * Defines the complete set of native function for Falcon.
 */
void defineNatives(FalconVM *vm) {
    for (unsigned long i = 0; i < sizeof(nativeFunctions) / sizeof(nativeFunctions[0]); i++)
        defNative(vm, nativeFunctions[i].name, nativeFunctions[i].function);
}
