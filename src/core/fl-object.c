/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * fl-object.c: Falcon's object representation
 * See Falcon's license in the LICENSE file
 */

#include "fl-object.h"
#include "../lib/fl-strlib.h"
#include "fl-mem.h"
#include <string.h>

/**
 * Returns the name, as a string, of a given ObjType.
 */
const char *get_object_name(ObjType type) {
    const char *objectTypeNames[] = {"OBJ_STRING", "OBJ_FUNCTION", "OBJ_UPVALUE", "OBJ_CLOSURE",
                                     "OBJ_CLASS",  "OBJ_INSTANCE", "OBJ_BMETHOD", "OBJ_LIST",
                                     "OBJ_MAP",    "OBJ_NATIVE"};
    return objectTypeNames[type];
}

/**
 * Creates a new empty ObjString of a given length by claiming ownership of the new string. In this
 * case, the characters of the ObjString can be later freed when no longer needed.
 */
ObjString *make_string(FalconVM *vm, size_t length) {
    ObjString *str = (ObjString *) allocate_object(vm, sizeof(ObjString) + length + 1, OBJ_STRING);
    str->length = length;
    return str;
}

/**
 * Copies and allocates a given string, of a given length, to the heap. This way, the resulting
 * ObjString reliably owns its character array and can later free it.
 */
ObjString *new_ObjString(FalconVM *vm, const char *chars, size_t length) {
    uint32_t hash = hash_string((const unsigned char *) chars, length);
    ObjString *interned = find_string(&vm->strings, chars, length, hash); /* Checks if interned */
    if (interned != NULL) return interned;

    /* Copies the characters to the ObjString */
    ObjString *str = make_string(vm, length);
    memcpy(str->chars, chars, length);
    str->chars[length] = '\0';
    str->hash = hash;

    DISABLE_GC(vm);                           /* Avoids GC from the "map_set" ahead */
    map_set(vm, &vm->strings, str, NULL_VAL); /* Interns the string */
    ENABLE_GC(vm);
    return str;
}

/**
 * Allocates a new empty ObjMap and initializes its fields.
 */
ObjMap *new_ObjMap(FalconVM *vm) {
    ObjMap *map = FALCON_ALLOCATE_OBJ(vm, ObjMap, OBJ_MAP);
    map->capacity = 0;
    map->count = 0;
    map->entries = NULL;
    return map;
}

/**
 * Allocates a new ObjFunction and initializes its fields.
 */
ObjFunction *new_ObjFunction(FalconVM *vm) {
    ObjFunction *function = FALCON_ALLOCATE_OBJ(vm, ObjFunction, OBJ_FUNCTION);
    function->arity = 0;
    function->upvalueCount = 0;
    function->name = NULL;
    init_bytecode(&function->bytecode);
    return function;
}

/**
 * Allocates a new ObjUpvalue and initializes its fields, setting the upvalue slot to a given
 * FalconValue.
 */
ObjUpvalue *new_ObjUpvalue(FalconVM *vm, FalconValue *slot) {
    ObjUpvalue *upvalue = FALCON_ALLOCATE_OBJ(vm, ObjUpvalue, OBJ_UPVALUE);
    upvalue->slot = slot;
    upvalue->next = NULL;
    upvalue->closed = NULL_VAL;
    return upvalue;
}

/**
 * Allocates a new ObjClosure and initializes its fields, also allocating a list of ObjUpvalues and
 * setting the closure function to a given ObjFunction.
 */
ObjClosure *new_ObjClosure(FalconVM *vm, ObjFunction *function) {
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
 * Allocates a new ObjClass and initializes its fields, setting its name to a given ObjString.
 */
ObjClass *new_ObjClass(FalconVM *vm, ObjString *name) {
    ObjClass *class_ = FALCON_ALLOCATE_OBJ(vm, ObjClass, OBJ_CLASS);
    DISABLE_GC(vm); /* Avoids GC from the "new_ObjMap" ahead */
    class_->name = name;
    class_->methods = *new_ObjMap(vm);
    ENABLE_GC(vm);
    return class_;
}

/**
 * Allocates a new ObjInstance and initializes its fields, setting its class to a given ObjClass.
 */
ObjInstance *new_ObjInstance(FalconVM *vm, ObjClass *class_) {
    ObjInstance *instance = FALCON_ALLOCATE_OBJ(vm, ObjInstance, OBJ_INSTANCE);
    DISABLE_GC(vm); /* Avoids GC from the "new_ObjMap" ahead */
    instance->class_ = class_;
    instance->fields = *new_ObjMap(vm);
    ENABLE_GC(vm);
    return instance;
}

/**
 * Allocates a new ObjBMethod and initializes its fields, setting its receiver to a given
 * FalconValue and its method to a given ObjClosure.
 */
ObjBMethod *new_ObjBMethod(FalconVM *vm, FalconValue receiver, ObjClosure *method) {
    ObjBMethod *bound = FALCON_ALLOCATE_OBJ(vm, ObjBMethod, OBJ_BMETHOD);
    bound->receiver = receiver;
    bound->method = method;
    return bound;
}

/**
 * Allocates a new ObjList and initializes its fields, also allocating a list of FalconValues of a
 * given size.
 */
ObjList *new_ObjList(FalconVM *vm, uint16_t size) {
    FalconValue *elements = FALCON_ALLOCATE(vm, FalconValue, size);
    ObjList *list = FALCON_ALLOCATE_OBJ(vm, ObjList, OBJ_LIST);
    list->elements.count = size;
    list->elements.capacity = size;
    list->elements.values = elements;
    return list;
}

/**
 * Allocates a new ObjNative and initializes its fields, setting its function to a given
 * FalconNativeFn and its name to a given string.
 */
ObjNative *new_ObjNative(FalconVM *vm, FalconNativeFn function, const char *name) {
    ObjNative *native = FALCON_ALLOCATE_OBJ(vm, ObjNative, OBJ_NATIVE);
    native->function = function;
    native->name = name;
    return native;
}
