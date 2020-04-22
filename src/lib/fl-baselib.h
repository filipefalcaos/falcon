/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * fl-baselib.h: Falcon's basic library
 * See Falcon's license in the LICENSE file
 */

#ifndef FALCON_BASELIB_H
#define FALCON_BASELIB_H

#include "../vm/fl-vm.h"

/* Basic library functions errors */
#define FALCON_CONV_STR_NUM_ERR "Could not convert string to number."

/* Prints to stdout (with a new line at the end) a given list of FalconValues. Any type of value in
 * Falcon can be printed and any number of arguments are accepted */
FalconValue lib_print(FalconVM *vm, int argCount, FalconValue *args);

/* Returns the type of a given FalconValue, as a ObjString. It takes only one argument, of any
 * Falcon type */
FalconValue lib_type(FalconVM *vm, int argCount, FalconValue *args);

/* Converts a given FalconValue to a boolean. It takes only one argument, of any Falcon type */
FalconValue lib_bool(FalconVM *vm, int argCount, FalconValue *args);

/* Converts a given FalconValue to a number. It takes only one argument that must be either a
 * string, a boolean, or numeric. If the conversion cannot be done, a error will be reported */
FalconValue lib_num(FalconVM *vm, int argCount, FalconValue *args);

/* Converts a given FalconValue to a ObjString. It takes only one argument, of any Falcon type */
FalconValue lib_str(FalconVM *vm, int argCount, FalconValue *args);

/* Returns the length of a given FalconValue. It takes only one argument that must be either a
 * string, a list, or a map. If the computing cannot be done, a error will be reported */
FalconValue lib_len(FalconVM *vm, int argCount, FalconValue *args);

/* Returns whether a given FalconValue has a given field. It takes two arguments: a class instance
 * and a ObjString (the field) */
FalconValue lib_hasField(FalconVM *vm, int argCount, FalconValue *args);

/* Returns the value of a given field from a given FalconValue. It takes two arguments: a class
 * instance and a ObjString (the field). If the field does not exist in the instance, an error will
 * be reported */
FalconValue lib_getField(FalconVM *vm, int argCount, FalconValue *args);

/* Sets a given FalconValue to a given field from another given FalconValue. It takes three
 * arguments: a class instance, a ObjString (the field), and a value to assign */
FalconValue lib_setField(FalconVM *vm, int argCount, FalconValue *args);

/* Deletes a given field from a given FalconValue. It takes two arguments: a class instance and a
 * ObjString (the field). Null is always returned */
FalconValue lib_delField(FalconVM *vm, int argCount, FalconValue *args);

#endif // FALCON_BASELIB_H
