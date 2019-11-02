/*
 * YAPL version 0.0.1 (master, Oct 18 2019)
 * yapl_object.h: YAPL's object representation
 * See YAPL's license in the LICENSE file
 */

#ifndef YAPL_OBJECT_H
#define YAPL_OBJECT_H

#include "../commons.h"
#include "yapl_bytecodechunk.h"
#include "yapl_value.h"

/* Types of objects on YAPL */
typedef enum { OBJ_STRING, OBJ_UPVALUE, OBJ_CLOSURE, OBJ_FUNCTION, OBJ_NATIVE } ObjType;

/* Structure of a YAPL object */
struct sObj {
    ObjType type;
    struct sObj *next;
};

/* YAPL's function object */
typedef struct {
    Obj obj;
    int arity;
    int upvalueCount;
    BytecodeChunk bytecodeChunk;
    ObjString *name;
} ObjFunction;

/* Native functions implementations */
typedef Value (*NativeFn)(int argCount, Value *args);

/* YAPL's native functions object */
typedef struct {
    Obj obj;
    NativeFn function;
} ObjNative;

/* YAPL's upvalue object */
typedef struct sUpvalue {
    Obj obj;
    Value *slot;
    Value closed;
    struct sUpvalue *next;
} ObjUpvalue;

/* YAPL's closure object */
typedef struct {
    Obj obj;
    ObjFunction *function;
    ObjUpvalue **upvalues;
    int upvalueCount;
} ObjClosure;

/* YAPL's string object */
struct sObjString {
    Obj obj;
    int length;
    uint32_t hash;
    char chars[]; /* Flexible array member */
};

/**
 * Checks if a Value is of an Obj type.
 */
static inline bool isObjType(Value value, ObjType type) {
    return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

/* Gets a object type from an YAPL Value */
#define OBJ_TYPE(value) (AS_OBJ(value)->type)

/* Checks if a Value is an Obj type */
#define IS_STRING(value)   isObjType(value, OBJ_STRING)

/* Gets the object value from a YAPL Value */
#define AS_CLOSURE(value)      ((ObjClosure *) AS_OBJ(value))
#define AS_FUNCTION(value)     ((ObjFunction *) AS_OBJ(value))
#define AS_NATIVE(value)       (((ObjNative *) AS_OBJ(value))->function)
#define AS_STRING(value)       ((ObjString *) AS_OBJ(value))
#define AS_CLANG_STRING(value) (((ObjString *) AS_OBJ(value))->chars)

/* Object operations */
void printObject(Value value);

/* Function/closure objects operations */
Obj *allocateObject(size_t size, ObjType type);
ObjUpvalue *newUpvalue(Value *slot);
ObjClosure* newClosure(ObjFunction *function);
ObjFunction *newFunction();
ObjNative *newNative(NativeFn function);

#endif // YAPL_OBJECT_H
