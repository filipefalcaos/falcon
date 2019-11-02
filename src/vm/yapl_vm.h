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
#include "yapl_object.h"

/* Call frame structure */
typedef struct {
    ObjClosure *closure; /* Running closure */
    uint8_t *pc;         /* Function's program counter */
    Value *slots;        /* Function's stack pointer */
} CallFrame;

/* YAPL's virtual machine structure */
typedef struct {
    const char *fileName;            /* The name of the running file */
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
#define UNKNOWN_OPCODE_ERR \
    "Unknown opcode %d. This is most likely a bug in YAPL itself. Please provide a bug report."
#define STACK_OVERFLOW      "Stack overflow."
#define UNDEF_VAR_ERR       "Undefined variable '%s'."
#define GLB_VAR_REDECL_ERR  "Global variable '%s' already declared."
#define ARGS_COUNT_ERR      "Expected %d arguments, but got %d."
#define ARGS_TYPE_ERR       "Expected argument %d to be a %s."
#define VALUE_NOT_CALL_ERR  "Cannot call value. Only functions and classes are callable values."
#define OPR_NOT_NUM_ERR     "Operand must be a number."
#define OPR_NOT_NUM_STR_ERR "Operands must be two numbers or two strings."

#endif // YAPL_VM_H
