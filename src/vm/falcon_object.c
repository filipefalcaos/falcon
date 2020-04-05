/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * falcon_object.h: Falcon's object representation
 * See Falcon's license in the LICENSE file
 */

#include "falcon_object.h"
#include "../lib/falcon_string.h"
#include "falcon_memory.h"
#include <string.h>

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
        "OBJ_INSTANCE",
        "OBJ_LIST",
        "OBJ_MAP",
        "OBJ_NATIVE"
    };

    return objectTypeNames[type];
}

/**
 * Creates a new ObjString by claiming ownership of the given string. In this case, the
 * characters of a ObjString can be freed when no longer needed.
 */
ObjString *makeString(FalconVM *vm, size_t length) {
    ObjString *str =
        (ObjString *) falconAllocateObj(vm, sizeof(ObjString) + length + 1, OBJ_STRING);
    str->length = length;
    return str;
}

/**
 * Copies and allocates a given string to the heap. This way, every ObjString reliably owns its
 * character array and can free it.
 */
ObjString *falconString(FalconVM *vm, const char *chars, size_t length) {
    uint32_t hash = hashString((const unsigned char *) chars, length);
    ObjString *interned = tableFindStr(&vm->strings, chars, length, hash); /* Checks if interned */
    if (interned != NULL) return interned;

    /* Copies the characters to the ObjString */
    ObjString *str = makeString(vm, length);
    memcpy(str->chars, chars, length);
    str->chars[length] = '\0';
    str->hash = hash;

    push(vm, OBJ_VAL(str));                    /* Avoids GC */
    tableSet(vm, &vm->strings, str, NULL_VAL); /* Interns the string */
    pop(vm);
    return str;
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
    initTable(&class_->methods);
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
 * Allocates a new Falcon bound method object.
 */
ObjBMethod *falconBMethod(FalconVM *vm, FalconValue receiver, ObjClosure *method) {
    ObjBMethod *bound = FALCON_ALLOCATE_OBJ(vm, ObjBMethod, OBJ_BMETHOD);
    bound->receiver = receiver;
    bound->method = method;
    return bound;
}

/**
 * Allocates a new Falcon list object.
 */
ObjList *falconList(FalconVM *vm, uint16_t size) {
    FalconValue *elements = FALCON_ALLOCATE(vm, FalconValue, size); /* Avoids GC */
    ObjList *list = FALCON_ALLOCATE_OBJ(vm, ObjList, OBJ_LIST);
    list->elements.count = size;
    list->elements.capacity = size;
    list->elements.values = elements;
    return list;
}

/**
 * Allocates a new Falcon map object.
 */
ObjMap *falconMap(FalconVM *vm, uint16_t length) {
    ObjMap *map = FALCON_ALLOCATE_OBJ(vm, ObjMap, OBJ_MAP);
    initTable(&map->entries);
    map->length = length;
    return map;
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
