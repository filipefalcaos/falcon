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
typedef enum { OBJ_STRING, OBJ_UPVALUE, OBJ_CLOSURE, OBJ_FUNCTION, OBJ_NATIVE } ObjType;

/* Object representation */
struct sObj {
    bool isMarked;
    ObjType type;
    struct sObj *next;
};

/* Falcon's string object */
struct _ObjString {
    FalconObj obj;
    size_t length;
    uint32_t hash;
    char chars[]; /* Flexible array member */
};

/* Falcon's upvalue object */
typedef struct _sUpvalue {
    FalconObj obj;
    FalconValue *slot;
    FalconValue closed;
    struct _sUpvalue *next;
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

/* Gets a object type from an Falcon Value */
#define FALCON_OBJ_TYPE(value) (FALCON_AS_OBJ(value)->type)

/* Checks if a Value is an FalconObj type */
#define FALCON_IS_STRING(value) falconIsObjType(value, OBJ_STRING)

/* Gets the object value from a Falcon Value */
#define FALCON_AS_CLOSURE(value)  ((ObjClosure *) FALCON_AS_OBJ(value))
#define FALCON_AS_FUNCTION(value) ((ObjFunction *) FALCON_AS_OBJ(value))
#define FALCON_AS_NATIVE(value)   ((ObjNative *) FALCON_AS_OBJ(value))
#define FALCON_AS_STRING(value)   ((ObjString *) FALCON_AS_OBJ(value))
#define FALCON_AS_CSTRING(value)  (((ObjString *) FALCON_AS_OBJ(value))->chars)

/* Object operations */
const char *falconGetObjName(ObjType type);
ObjUpvalue *falconUpvalue(FalconVM *vm, FalconValue *slot);
ObjClosure *falconClosure(FalconVM *vm, ObjFunction *function);
ObjFunction *falconFunction(FalconVM *vm);
ObjNative *falconNative(FalconVM *vm, FalconNativeFn function, const char *name);
void falconPrintObj(FalconValue value);

/**
 * Checks if a Value is of an FalconObj type.
 */
static inline bool falconIsObjType(FalconValue value, ObjType type) {
    return FALCON_IS_OBJ(value) && FALCON_AS_OBJ(value)->type == type;
}

#endif // FALCON_OBJECT_H
