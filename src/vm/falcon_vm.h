/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * falcon_vm.h: Falcon's stack-based virtual machine
 * See Falcon's license in the LICENSE file
 */

#ifndef FALCON_VM_H
#define FALCON_VM_H

#include "../compiler/falcon_compiler.h"
#include "../falcon.h"
#include "../lib/falcon_table.h"
#include "falcon_bytecode.h"
#include "falcon_object.h"

/* Call frame representation */
typedef struct {
    ObjClosure *closure; /* Running closure */
    uint8_t *pc;         /* Function's program counter */
    FalconValue *slots;  /* Function's stack pointer */
} CallFrame;

/* Falcon's virtual machine representation */
struct FalconVM {

    /* Basic info on the running program: (i) the name of the running file; (ii) whether it is
     * running on REPL or not; and (iii) whether opcodes should be displayed or not */
    const char *fileName;
    bool isREPL;
    bool dumpOpcodes;

    /* Bytecode chunk to interpret and the program counter */
    BytecodeChunk *bytecode;
    uint8_t *pc;

    /* VM's call frames */
    CallFrame frames[FALCON_FRAMES_MAX];
    int frameCount;

    /* VM's stack and it's pointer to the stack top */
    FalconValue stack[FALCON_STACK_MAX];
    FalconValue *stackTop;

    /* List of open upvalues */
    ObjUpvalue *openUpvalues;

    /* List of runtime objects */
    FalconObj *objects;

    /* Table for the interned strings */
    Table strings;

    /* Table for all global variables */
    Table globals;

    /* Current function compiler. This is necessary when the garbage collector is triggered during
     * the compilation stage */
    FunctionCompiler *compiler;

    /* The stack of unprocessed objects (i.e., "greys") while garbage collection is in process */
    int grayCount;
    int grayCapacity;
    FalconObj **grayStack;

    /* The total number of bytes of managed memory the VM has allocated and the memory threshold
     * that triggers the next garbage collection */
    size_t bytesAllocated;
    size_t nextGC;

    /* Default string to store the "init" initializer name */
    ObjString *initStr;
};

/* Interpretation result codes */
typedef enum { FALCON_OK, FALCON_COMPILE_ERROR, FALCON_RUNTIME_ERROR } FalconResultCode;

/* Virtual machine operations */
void initFalconVM(FalconVM *vm);
void freeFalconVM(FalconVM *vm);
void resetStack(FalconVM *vm);
bool push(FalconVM *vm, FalconValue value);
FalconValue pop(FalconVM *vm);
FalconResultCode falconInterpret(FalconVM *vm, const char *source);

/* The initial allocation size for the heap, in bytes */
#define VM_BASE_HEAP_SIZE 1000000 /* 1Mb */

/* Runtime error messages */
/* VM Bug */
#define VM_BUG_REPORT         "Please provide a bug report."
#define VM_BUG                "This is most likely a bug in Falcon itself. " VM_BUG_REPORT
#define VM_UNKNOWN_OPCODE_ERR "Unknown opcode %d. " VM_BUG
#define VM_UNREACHABLE_ERR    "Opcode %d should be unreachable. " VM_BUG

/* Overflow and Limits */
#define VM_STACK_OVERFLOW "Stack overflow."

/* Variables */
#define VM_UNDEF_VAR_ERR "Undefined variable '%s'."

/* Classes */
#define VM_UNDEF_PROP_ERR   "Undefined property '%s.%s'."
#define VM_NOT_INSTANCE_ERR "Only instances of classes have properties."
#define VM_INIT_ERR         "Class has no initializer, but %d arguments were given."
#define VM_INHERITANCE_ERR  "Cannot inherit from a value that is not a class."

/* Functions */
#define VM_ARGS_COUNT_ERR     "Expected %d arguments, but got %d."
#define VM_ARGS_TYPE_ERR      "Expected argument %d to be a %s."
#define VM_VALUE_NOT_CALL_ERR "Cannot call value."

/* Operands */
#define VM_OPR_NOT_NUM_ERR     "Operand must be a number."
#define VM_OPR_NOT_NUM_STR_ERR "Operands must be two numbers or two strings."
#define VM_DIV_ZERO_ERR        "Cannot perform a division by zero."

/* Indexing */
#define VM_INDEX_NOT_NUM_ERR "List index must be a number."
#define VM_INDEX_ERR         "Indexed value must be a list or a string."
#define VM_INDEX_ASSG_ERR    "Only lists support subscript assignment."
#define VM_LIST_BOUNDS_ERR   "List index out of bounds."
#define VM_STRING_BOUNDS_ERR "String index out of bounds."

#endif // FALCON_VM_H
