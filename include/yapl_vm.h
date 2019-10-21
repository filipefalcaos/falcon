/*
 * YAPL version 0.0.1 (master, Oct 18 2019)
 * yapl_vm.h: YAPL's stack-based virtual machine
 * See YAPL's license in the LICENSE file
 */

#ifndef YAPL_VM_H
#define YAPL_VM_H

#include "yapl.h"
#include "yapl_bytecode_chunk.h"
#include "yapl_object.h"
#include "yapl_table.h"

/* Call frame structure */
typedef struct {
    ObjFunction *function; /* Running function */
    uint8_t *pc;           /* Function's program counter */
    Value *slots;          /* Function's stack pointer */
} CallFrame;

/* YAPL's virtual machine structure */
typedef struct {
    CallFrame frames[VM_FRAMES_MAX]; /* VM's call frames */
    int frameCount;                  /* Call frames count */
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

/* VM instance */
extern VM vm;

/* Virtual machine operations */
void initVM();
void freeVM();
void push(Value value);
Value pop();
ResultCode interpret(const char *source);

#endif // YAPL_VM_H
