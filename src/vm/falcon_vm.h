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

/* Types of objects on Falcon */
typedef enum { OBJ_STRING, OBJ_UPVALUE, OBJ_CLOSURE, OBJ_FUNCTION, OBJ_NATIVE } FalconObjType;

/* Object representation */
struct sObj {
    FalconObjType type;
    struct sObj *next;
};

/* Falcon's string object */
struct sObjString {
    FalconObj obj;
    int length;
    uint32_t hash;
    char chars[]; /* Flexible array member */
};

/* Falcon's function object */
typedef struct {
    FalconObj obj;
    int arity;
    int upvalueCount;
    FalconBytecodeChunk bytecodeChunk;
    FalconObjString *name;
} FalconObjFunction;

/* Falcon's upvalue object */
typedef struct sUpvalue {
    FalconObj obj;
    FalconValue *slot;
    FalconValue closed;
    struct sUpvalue *next;
} FalconObjUpvalue;

/* Falcon's closure object */
typedef struct {
    FalconObj obj;
    FalconObjFunction *function;
    FalconObjUpvalue **upvalues;
    int upvalueCount;
} FalconObjClosure;

/**
 * Checks if a Value is of an FalconObj type.
 */
static inline bool isObjType(FalconValue value, FalconObjType type) {
    return FALCON_IS_OBJ(value) && FALCON_AS_OBJ(value)->type == type;
}

/* Gets a object type from an Falcon Value */
#define OBJ_TYPE(value) (FALCON_AS_OBJ(value)->type)

/* Checks if a Value is an FalconObj type */
#define IS_STRING(value)   isObjType(value, OBJ_STRING)

/* Gets the object value from a Falcon Value */
#define AS_CLOSURE(value)      ((FalconObjClosure *) FALCON_AS_OBJ(value))
#define AS_FUNCTION(value)     ((FalconObjFunction *) FALCON_AS_OBJ(value))
#define AS_NATIVE(value)       (((FalconObjNative *) FALCON_AS_OBJ(value))->function)
#define AS_STRING(value)       ((FalconObjString *) FALCON_AS_OBJ(value))
#define AS_CLANG_STRING(value) (((FalconObjString *) FALCON_AS_OBJ(value))->chars)

/* Call frame representation */
typedef struct {
    FalconObjClosure *closure; /* Running closure */
    uint8_t *pc;               /* Function's program counter */
    FalconValue *slots;        /* Function's stack pointer */
} FalconCallFrame;

/* Falcon's virtual machine representation */
typedef struct {
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
} VM;

/* Interpretation result codes */
typedef enum { FALCON_OK, FALCON_COMPILE_ERROR, FALCON_RUNTIME_ERROR } FalconResultCode;

/* Virtual machine operations */
void FalconVMError(VM *vm, const char *format, ...);
void FalconInitVM(VM *vm);
void FalconFreeVM(VM *vm);
bool FalconPush(VM *vm, FalconValue value);
FalconValue FalconPop(VM *vm);
FalconResultCode FalconInterpret(VM *vm, const char *source);

/* Function/closure objects operations */
FalconObjUpvalue *FalconNewUpvalue(VM *vm, FalconValue *slot);
FalconObjClosure *FalconNewClosure(VM *vm, FalconObjFunction *function);
FalconObjFunction *FalconNewFunction(VM *vm);

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
