/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * falcon_vm.h: Falcon's stack-based virtual machine
 * See Falcon's license in the LICENSE file
 */

#ifndef FALCON_VM_H
#define FALCON_VM_H

#include "../falcon.h"
#include "../lib/falcon_table.h"
#include "falcon_bytecode.h"
#include "falcon_object.h"

/* Call frame representation */
typedef struct {
    FalconObjClosure *closure; /* Running closure */
    uint8_t *pc;               /* Function's program counter */
    FalconValue *slots;        /* Function's stack pointer */
} FalconCallFrame;

/* Falcon's virtual machine representation */
struct FalconVM {
    const char *fileName;                         /* The name of the running file */
    bool isREPL;                                  /* Whether is running on REPL */
    FalconCallFrame frames[FALCON_VM_FRAMES_MAX]; /* VM's call frames */
    int frameCount;                               /* Call frames count */
    FalconObjUpvalue *openUpvalues;               /* List of open upvalues */
    FalconBytecodeChunk *bytecodeChunk;           /* Bytecode chunk to interpret */
    uint8_t *pc;                                  /* Program counter */
    FalconValue stack[FALCON_VM_STACK_MAX];       /* VM's stack */
    FalconValue *stackTop;                        /* Pointer to the stack top */
    FalconObj *objects;                           /* List of runtime objects */
    FalconTable strings;                          /* Strings table */
    FalconTable globals;                          /* Global variables */
};

/* Interpretation result codes */
typedef enum { FALCON_OK, FALCON_COMPILE_ERROR, FALCON_RUNTIME_ERROR } FalconResultCode;

/* Virtual machine operations */
void FalconVMError(FalconVM *vm, const char *format, ...);
void FalconInitVM(FalconVM *vm);
void FalconFreeVM(FalconVM *vm);
bool FalconPush(FalconVM *vm, FalconValue value);
FalconValue FalconPop(FalconVM *vm);
FalconResultCode FalconInterpret(FalconVM *vm, const char *source);

/* Runtime error messages */
#define FALCON_BUG \
    "This is most likely a bug in Falcon itself. Please provide a bug report."
#define FALCON_UNKNOWN_OPCODE_ERR  "Unknown opcode %d. " FALCON_BUG
#define FALCON_UNREACHABLE_ERR     "Opcode %d should be unreachable. " FALCON_BUG
#define FALCON_STACK_OVERFLOW      "Stack overflow."
#define FALCON_UNDEF_VAR_ERR       "Undefined variable '%s'."
#define FALCON_ARGS_COUNT_ERR      "Expected %d arguments, but got %d."
#define FALCON_ARGS_TYPE_ERR       "Expected argument %d to be a %s."
#define FALCON_VALUE_NOT_CALL_ERR \
    "Cannot call value. Only functions and classes are callable values."
#define FALCON_OPR_NOT_NUM_ERR     "Operand must be a number."
#define FALCON_OPR_NOT_NUM_STR_ERR "Operands must be two numbers or two strings."
#define FALCON_DIV_ZERO_ERR        "Cannot perform a division by zero."

#endif // FALCON_VM_H
