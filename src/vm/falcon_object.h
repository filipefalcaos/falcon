/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * falcon_object.h: Falcon's object representation
 * See Falcon's license in the LICENSE file
 */

#ifndef FALCON_OBJECT_H
#define FALCON_OBJECT_H

#include "falcon_value.h"
#include "falcon_bytecode.h"

/* Types of objects on Falcon */
typedef enum {
    FALCON_OBJ_STRING,
    FALCON_OBJ_UPVALUE,
    FALCON_OBJ_CLOSURE,
    FALCON_OBJ_FUNCTION,
    FALCON_OBJ_NATIVE
} FalconObjType;

/* Object representation */
struct sObj {
    FalconObjType type;
    struct sObj *next;
};

/* Falcon's string object */
struct sObjString {
    FalconObj obj;
    int length;
    uint32_t hash;
    char chars[]; /* Flexible array member */
};

/* Falcon's upvalue object */
typedef struct sUpvalue {
    FalconObj obj;
    FalconValue *slot;
    FalconValue closed;
    struct sUpvalue *next;
} FalconObjUpvalue;

/* Falcon's function object */
typedef struct {
    FalconObj obj;
    int arity;
    int upvalueCount;
    FalconBytecodeChunk bytecodeChunk;
    FalconObjString *name;
} FalconObjFunction;

/* Falcon's closure object */
typedef struct {
    FalconObj obj;
    FalconObjFunction *function;
    FalconObjUpvalue **upvalues;
    int upvalueCount;
} FalconObjClosure;

/* Native functions implementations */
typedef FalconValue (*FalconNativeFn)(FalconVM *vm, int argCount, FalconValue *args);

/* Falcon's native functions object */
typedef struct {
    FalconObj obj;
    FalconNativeFn function;
    const char *functionName;
} FalconObjNative;

/* Gets a object type from an Falcon Value */
#define FALCON_OBJ_TYPE(value) (FALCON_AS_OBJ(value)->type)

/* Checks if a Value is an FalconObj type */
#define FALCON_IS_STRING(value) isObjType(value, FALCON_OBJ_STRING)

/* Gets the object value from a Falcon Value */
#define FALCON_AS_CLOSURE(value)  ((FalconObjClosure *) FALCON_AS_OBJ(value))
#define FALCON_AS_FUNCTION(value) ((FalconObjFunction *) FALCON_AS_OBJ(value))
#define FALCON_AS_NATIVE(value)   ((FalconObjNative *) FALCON_AS_OBJ(value))
#define FALCON_AS_STRING(value)   ((FalconObjString *) FALCON_AS_OBJ(value))
#define FALCON_AS_CSTRING(value)  (((FalconObjString *) FALCON_AS_OBJ(value))->chars)

/* Object operations */
FalconObjUpvalue *FalconNewUpvalue(FalconVM *vm, FalconValue *slot);
FalconObjClosure *FalconNewClosure(FalconVM *vm, FalconObjFunction *function);
FalconObjFunction *FalconNewFunction(FalconVM *vm);
FalconObjNative *FalconNewNative(FalconVM *vm, FalconNativeFn function, const char *name);
void FalconPrintObject(FalconValue value);

/**
 * Checks if a Value is of an FalconObj type.
 */
static inline bool isObjType(FalconValue value, FalconObjType type) {
    return FALCON_IS_OBJ(value) && FALCON_AS_OBJ(value)->type == type;
}

#endif // FALCON_OBJECT_H
