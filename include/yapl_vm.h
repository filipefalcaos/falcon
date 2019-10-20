/*
 * YAPL version 0.0.1 (master, Oct 18 2019)
 * yapl_vm.h: YAPL's stack-based virtual machine
 * See YAPL's license in the LICENSE file
 */

#ifndef YAPL_VM_H
#define YAPL_VM_H

#include "yapl.h"
#include "yapl_bytecode_chunk.h"

/* YAPL's virtual machine structure */
typedef struct {
    BytecodeChunk *bytecodeChunk;      /* Bytecode chunk to interpret */
    uint8_t *pc;                       /* Program counter */
    Value stack[YAPL_MAX_SINGLE_BYTE]; /* VM stack */
    Value *stackTop;                   /* Pointer to the stack top */
} VM;

/* Interpretation result codes */
typedef enum { OK, COMPILE_ERROR, RUNTIME_ERROR } ResultCode;

/* Virtual machine operations */
void initVM();
void freeVM();
void push(Value value);
Value pop();
ResultCode interpret(const char *source);

#endif // YAPL_VM_H
