/*
 * YAPL version 0.0.1 (master, Oct 18 2019)
 * yapl_object.h: YAPL's object representation
 * See YAPL's license in the LICENSE file
 */

#ifndef YAPL_OBJECT_H
#define YAPL_OBJECT_H

#include "commons.h"
#include "yapl_value.h"

/* Types of objects on YAPL */
typedef enum { OBJ_STRING } ObjType;

/* Structure of a YAPL object */
struct sObj {
    ObjType type;
    struct sObj *next;
};

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
#define IS_STRING(value) isObjType(value, OBJ_STRING)

/* Gets the object value from a YAPL Value */
#define AS_STRING(value)       ((ObjString *) AS_OBJ(value))
#define AS_CLANG_STRING(value) (((ObjString *) AS_OBJ(value))->chars)

/* Object operations */
void printObject(Value value);
uint32_t hashString(const char *key, int length);
ObjString *makeString(int length);
ObjString *copyString(const char *chars, int length);

#endif // YAPL_OBJECT_H
