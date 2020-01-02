/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * falcon_object.h: Falcon's object representation
 * See Falcon's license in the LICENSE file
 */

#include "falcon_object.h"
#include "falcon_memory.h"

/**
 * Gets the name (string) of a given Falcon Object type.
 */
const char *getObjName(ObjType type) {
    const char *objectTypeNames[] = {
        "OBJ_STRING",
        "OBJ_FUNCTION",
        "OBJ_UPVALUE",
        "OBJ_CLOSURE",
        "OBJ_CLASS",
        "OBJ_LIST",
        "OBJ_NATIVE"
    };
    return objectTypeNames[type];
}

/**
 * Allocates a new Falcon function object.
 */
ObjFunction *falconFunction(FalconVM *vm) {
    ObjFunction *function = FALCON_ALLOCATE_OBJ(vm, ObjFunction, OBJ_FUNCTION);
    function->arity = 0;
    function->upvalueCount = 0;
    function->name = NULL;
    initBytecode(&function->bytecode);
    return function;
}

/**
 * Allocates a new Falcon upvalue object.
 */
ObjUpvalue *falconUpvalue(FalconVM *vm, FalconValue *slot) {
    ObjUpvalue *upvalue = FALCON_ALLOCATE_OBJ(vm, ObjUpvalue, OBJ_UPVALUE);
    upvalue->slot = slot;
    upvalue->next = NULL;
    upvalue->closed = NULL_VAL;
    return upvalue;
}

/**
 * Allocates a new Falcon closure object.
 */
ObjClosure *falconClosure(FalconVM *vm, ObjFunction *function) {
    ObjUpvalue **upvalues =
        FALCON_ALLOCATE(vm, ObjUpvalue *, function->upvalueCount); /* Sets upvalue list */

    for (int i = 0; i < function->upvalueCount; i++) {
        upvalues[i] = NULL; /* Initialize current upvalue */
    }

    ObjClosure *closure = FALCON_ALLOCATE_OBJ(vm, ObjClosure, OBJ_CLOSURE);
    closure->function = function;
    closure->upvalues = upvalues;
    closure->upvalueCount = function->upvalueCount;
    return closure;
}

/**
 * Allocates a new Falcon class object.
 */
ObjClass *falconClass(FalconVM *vm, ObjString *name) {
    ObjClass *class_ = FALCON_ALLOCATE_OBJ(vm, ObjClass, OBJ_CLASS);
    class_->name = name;
    return class_;
}

/**
 * Allocates a new Falcon instance (of a class) object.
 */
ObjInstance *falconInstance(FalconVM *vm, ObjClass *class_) {
    ObjInstance *instance = FALCON_ALLOCATE_OBJ(vm, ObjInstance, OBJ_INSTANCE);
    instance->class_ = class_;
    initTable(&instance->fields);
    return instance;
}

/**
 * Allocates a new Falcon list object.
 */
ObjList *falconList(FalconVM *vm, int size) {
    ObjList *list = FALCON_ALLOCATE_OBJ(vm, ObjList, OBJ_LIST);
    list->elements.count = 0;
    list->elements.capacity = size;
    list->elements.values = FALCON_ALLOCATE(vm, FalconValue, size);
    return list;
}

/**
 * Allocates a new Falcon native function object.
 */
ObjNative *falconNative(FalconVM *vm, FalconNativeFn function, const char *name) {
    ObjNative *native = FALCON_ALLOCATE_OBJ(vm, ObjNative, OBJ_NATIVE);
    native->function = function;
    native->name = name;
    return native;
}
