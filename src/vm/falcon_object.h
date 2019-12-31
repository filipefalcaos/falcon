/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * falcon_object.h: Falcon's object representation
 * See Falcon's license in the LICENSE file
 */

#ifndef FALCON_OBJECT_H
#define FALCON_OBJECT_H

#include "falcon_bytecode.h"
#include "falcon_value.h"

/* Types of objects on Falcon */
typedef enum { OBJ_STRING, OBJ_UPVALUE, OBJ_CLOSURE, OBJ_FUNCTION, OBJ_NATIVE, OBJ_LIST } ObjType;

/* Object representation */
struct _Obj {
    bool isMarked;
    ObjType type;
    struct _Obj *next;
};

/* Falcon's string object */
struct _ObjString {
    FalconObj obj;
    size_t length;
    uint32_t hash;
    char chars[]; /* Flexible array member */
};

/* Falcon's upvalue object */
typedef struct _ObjUpvalue {
    FalconObj obj;
    FalconValue *slot;
    FalconValue closed;
    struct _ObjUpvalue *next;
} ObjUpvalue;

/* Falcon's function object */
typedef struct {
    FalconObj obj;
    int arity;
    int upvalueCount;
    BytecodeChunk bytecode;
    ObjString *name;
} ObjFunction;

/* Falcon's closure object */
typedef struct {
    FalconObj obj;
    ObjFunction *function;
    ObjUpvalue **upvalues;
    int upvalueCount;
} ObjClosure;

/* Native functions implementations */
typedef FalconValue (*FalconNativeFn)(FalconVM *vm, int argCount, FalconValue *args);

/* Falcon's native functions object */
typedef struct {
    FalconObj obj;
    FalconNativeFn function;
    const char *name;
} ObjNative;

/* Falcon's list object */
typedef struct {
    FalconObj obj;
    ValueArray elements;
} ObjList;

/* Gets a object type from an Falcon Value */
#define OBJ_TYPE(value) (AS_OBJ(value)->type)

/* Checks if a Value is an FalconObj type */
#define IS_STRING(value) isObjType(value, OBJ_STRING)
#define IS_LIST(value)   isObjType(value, OBJ_LIST)

/* Gets the object value from a Falcon Value */
#define AS_CLOSURE(value)  ((ObjClosure *) AS_OBJ(value))
#define AS_FUNCTION(value) ((ObjFunction *) AS_OBJ(value))
#define AS_NATIVE(value)   ((ObjNative *) AS_OBJ(value))
#define AS_STRING(value)   ((ObjString *) AS_OBJ(value))
#define AS_CSTRING(value)  (((ObjString *) AS_OBJ(value))->chars)
#define AS_LIST(value)     ((ObjList *) AS_OBJ(value))

/* Object operations */
const char *getObjName(ObjType type);
ObjUpvalue *falconUpvalue(FalconVM *vm, FalconValue *slot);
ObjClosure *falconClosure(FalconVM *vm, ObjFunction *function);
ObjFunction *falconFunction(FalconVM *vm);
ObjNative *falconNative(FalconVM *vm, FalconNativeFn function, const char *name);
ObjList *falconList(FalconVM *vm, int size);

/**
 * Checks if a Value is of an FalconObj type.
 */
static inline bool isObjType(FalconValue value, ObjType type) {
    return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

#endif // FALCON_OBJECT_H
