/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * fl-vm.h: Falcon's stack-based virtual machine
 * See Falcon's license in the LICENSE file
 */

#ifndef FL_VM_H
#define FL_VM_H

#include "../falcon.h"
#include "../lib/fl-maplib.h"
#include "fl-bcode.h"
#include "fl-compiler.h"
#include "fl-object.h"

/* A call frame defines a "frame" within the VM's stack where the local variables of a function can
 * be stored. A call frame will contain, the receiver, the function's parameters, the locals, and,
 * finally, the temporaries (in that order) */
typedef struct {
    ObjClosure *closure; /* The Running closure */
    uint8_t *pc;         /* A Pointer to the current instruction in the function's bytecode */
    FalconValue *slots;  /* A pointer to the start of the call frame within the VM's stack */
} CallFrame;

/* The Falcon's stack-based virtual machine */
struct FalconVM {

    /* Basic info on the running program: (i) the name of the running file; and (ii) whether it is
     * running on REPL or not */
    const char *fileName;
    bool isREPL;

    /* Debugging options: (i) whether opcodes should be displayed or not; (ii) whether the
     * execution should de traced or not; and (iii) whether the memory allocation should be traced
     * or not */
    bool dumpOpcodes;
    bool traceExec;
    bool traceMemory;

    /* Call frames: (i) the stack of call frames in the VM; and (ii) the current count of call
     * frames in use */
    CallFrame frames[FALCON_FRAMES_MAX];
    int frameCount;

    /* Value stack: (i) the stack of values of the VM; and (ii) a pointer to the top of VM's stack
     * of values */
    FalconValue stack[FALCON_STACK_MAX];
    FalconValue *stackTop;

    /* A pointer to the first node on the list of open upvalues (pointing to values that are still
     * on the value stack) */
    ObjUpvalue *openUpvalues;

    /* A pointer to the first node on the list of runtime objects */
    FalconObj *objects;

    /* A hashtable for all the strings that are allocated in the runtime */
    ObjMap strings;

    /* A hashtable for all the declared global variables */
    ObjMap globals;

    /* The current function compiler (necessary if the garbage collector is triggered during the
     * compilation stage) */
    FunctionCompiler *compiler;

    /* "Gray" set: (i) the current count of objects in the "gray" stack; (ii) the current capacity
     * of the "gray" stack; and (iii) the stack of "gray" objects while a garbage collection run is
     * in process */
    int grayCount;
    int grayCapacity;
    FalconObj **grayStack;

    /* Memory management: (i) the total number of bytes of memory the VM has allocated; (ii) the
     * memory threshold that triggers the next garbage collection run; and (iii) whether the
     * garbage collector is enabled or not */
    size_t bytesAllocated;
    size_t nextGC;
    bool gcEnabled;

    /* The string object to store the default initializer name (i.e., "init") */
    ObjString *initStr;
};

/* Interpretation result codes */
typedef enum { FALCON_OK, FALCON_COMPILE_ERROR, FALCON_RUNTIME_ERROR } FalconResultCode;

/* Prints a runtime error to stderr and resets the virtual machine stack. The error message is
 * composed of a given format, followed by the arguments of that format */
void interpreter_error(FalconVM *vm, const char *format, ...);

/* Initializes the Falcon's virtual machine */
void init_FalconVM(FalconVM *vm);

/* Frees the Falcon's virtual machine and its allocated objects */
void free_FalconVM(FalconVM *vm);

/* Interprets a Falcon's source code string */
FalconResultCode interpret_source(FalconVM *vm, const char *source);

/* Resets the Falcon's virtual machine stack */
void reset_stack(FalconVM *vm);

/* Pushes a value to the top of the Falcon's virtual machine stack */
bool push(FalconVM *vm, FalconValue value);

/* Pops a value from the top of the Falcon's virtual machine stack by decreasing the "stackTop"
 * pointer */
FalconValue pop(FalconVM *vm);

/* Checks the validity of a given argument count based on an expected argument count */
#define ASSERT_ARGS_COUNT(vm, op, argCount, expectedCount)                     \
    do {                                                                       \
        if (argCount op expectedCount) {                                       \
            interpreter_error(vm, VM_ARGS_COUNT_ERR, expectedCount, argCount); \
            return ERR_VAL;                                                    \
        }                                                                      \
    } while (false)

/* Checks if a given value "value" of a given type "type" at a given argument position "pos" is a
 * value of the requested type */
#define ASSERT_ARG_TYPE(type, typeName, value, vm, pos)             \
    do {                                                            \
        if (!type(value)) {                                         \
            interpreter_error(vm, VM_ARGS_TYPE_ERR, pos, typeName); \
            return ERR_VAL;                                         \
        }                                                           \
    } while (false)

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
#define VM_DIV_ZERO_ERR        "Divisor must be a non-zero number."

/* Indexing */
#define VM_LIST_INDEX_ERR    "List index must be a number."
#define VM_MAP_INDEX_ERR     "Map key must be a string."
#define VM_STRING_INDEX_ERR  "String index must be a number."
#define VM_INDEX_ERR         "Indexed value must be a list, a map or a string."
#define VM_INDEX_ASSG_ERR    "Only lists and maps support subscript assignment."
#define VM_LIST_BOUNDS_ERR   "List index out of bounds."
#define VM_STRING_BOUNDS_ERR "String index out of bounds."
#define VM_STRING_MUT_ERR    "String content cannot be modified."

#endif // FL_VM_H
