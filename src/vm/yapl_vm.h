/*
 * YAPL version 0.0.1 (master, Oct 18 2019)
 * yapl_vm.h: YAPL's stack-based virtual machine
 * See YAPL's license in the LICENSE file
 */

#ifndef YAPL_VM_H
#define YAPL_VM_H

#include "../lib/yapl_table.h"
#include "../yapl.h"
#include "yapl_bytecodechunk.h"

/* Types of objects on YAPL */
typedef enum { OBJ_STRING, OBJ_UPVALUE, OBJ_CLOSURE, OBJ_FUNCTION, OBJ_NATIVE } ObjType;

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

/* YAPL's function object */
typedef struct {
    Obj obj;
    int arity;
    int upvalueCount;
    BytecodeChunk bytecodeChunk;
    ObjString *name;
} ObjFunction;

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

/* Call frame structure */
typedef struct {
    ObjClosure *closure; /* Running closure */
    uint8_t *pc;         /* Function's program counter */
    Value *slots;        /* Function's stack pointer */
} CallFrame;

/* YAPL's virtual machine structure */
typedef struct {
    const char *fileName;            /* The name of the running file */
    bool isREPL;                     /* Whether is running on REPL */
    CallFrame frames[VM_FRAMES_MAX]; /* VM's call frames */
    int frameCount;                  /* Call frames count */
    ObjUpvalue *openUpvalues;        /* List of open upvalues */
    BytecodeChunk *bytecodeChunk;    /* Bytecode chunk to interpret */
    uint8_t *pc;                     /* Program counter */
    Value stack[VM_STACK_MAX];       /* VM's stack */
    Value *stackTop;                 /* Pointer to the stack top */
    Obj *objects;                    /* List of runtime objects */
    Table strings;                   /* Strings table */
    Table globals;                   /* Global variables */
} VM;

/* Interpretation result codes */
typedef enum { OK, COMPILE_ERROR, RUNTIME_ERROR } ResultCode;

/* Virtual machine operations */
void VMError(VM *vm, const char *format, ...);
void initVM(VM *vm);
void freeVM(VM *vm);
bool push(VM *vm, Value value);
Value pop(VM *vm);
ResultCode interpret(VM *vm, const char *source);

/* Function/closure objects operations */
ObjUpvalue *newUpvalue(VM *vm, Value *slot);
ObjClosure* newClosure(VM *vm, ObjFunction *function);
ObjFunction *newFunction(VM *vm);

/* Runtime error messages */
#define YAPL_BUG            "This is most likely a bug in YAPL itself. Please provide a bug report."
#define UNKNOWN_OPCODE_ERR  "Unknown opcode %d. " YAPL_BUG
#define UNREACHABLE_ERR     "Opcode %d should be unreachable. " YAPL_BUG
#define STACK_OVERFLOW      "Stack overflow."
#define UNDEF_VAR_ERR       "Undefined variable '%s'."
#define ARGS_COUNT_ERR      "Expected %d arguments, but got %d."
#define ARGS_TYPE_ERR       "Expected argument %d to be a %s."
#define VALUE_NOT_CALL_ERR  "Cannot call value. Only functions and classes are callable values."
#define OPR_NOT_NUM_ERR     "Operand must be a number."
#define OPR_NOT_NUM_STR_ERR "Operands must be two numbers or two strings."
#define DIV_ZERO_ERR        "Cannot perform a division by zero."

#endif // YAPL_VM_H
