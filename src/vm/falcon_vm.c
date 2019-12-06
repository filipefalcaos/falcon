/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * falcon_vm.c: Falcon's stack-based virtual machine
 * See Falcon's license in the LICENSE file
 */

#include "falcon_vm.h"
#include "../compiler/falcon_compiler.h"
#include "../lib/falcon_error.h"
#include "../lib/falcon_math.h"
#include "../lib/falcon_natives.h"
#include "../lib/falcon_string.h"
#include "falcon_memory.h"
#include <stdarg.h>
#include <stdio.h>

#ifdef FALCON_DEBUG_TRACE_EXECUTION
#include "falcon_debug.h"
#endif

/* The initial allocation size for the heap, in bytes */
#define FALCON_BASE_HEAP_SIZE 1000000 /* 1Mb */

/**
 * Resets the virtual machine stack.
 */
static void resetVMStack(FalconVM *vm) {
    vm->stackTop = vm->stack;
    vm->openUpvalues = NULL;
    vm->frameCount = 0;
}

/**
 * Presents a runtime error to the programmer and resets the VM stack.
 */
void FalconVMError(FalconVM *vm, const char *format, ...) {
    va_list args;
    va_start(args, format);
    FalconRuntimeError(vm, format, args); /* Presents the error */
    va_end(args);
    resetVMStack(vm); /* Resets the stack due to error */
}

/**
 * Presents a runtime error when a global variable is undefined.
 */
static FalconResultCode undefinedVariableError(FalconVM *vm, FalconObjString *name, bool delete) {
    if (delete) FalconTableDelete(&vm->globals, name);
    FalconVMError(vm, FALCON_UNDEF_VAR_ERR, name->chars);
    return FALCON_RUNTIME_ERROR;
}

/**
 * Initializes the Falcon's virtual machine.
 */
void FalconInitVM(FalconVM *vm) {
    resetVMStack(vm); /* Inits the VM stack */

    /* Inits the VM fields */
    vm->fileName = NULL;
    vm->isREPL = false;
    vm->objects = NULL;

    /* Inits the garbage collection fields */
    vm->grayCount = 0;
    vm->grayCapacity = 0;
    vm->grayStack = NULL;
    vm->bytesAllocated = 0;
    vm->nextGC = FALCON_BASE_HEAP_SIZE;

    FalconInitTable(&vm->strings); /* Inits the table of interned strings */
    FalconInitTable(&vm->globals); /* Inits the table of globals */
    FalconDefineNatives(vm);       /* Sets native functions */
}

/**
 * Frees the Falcon's virtual machine and its allocated objects.
 */
void FalconFreeVM(FalconVM *vm) {
    FalconFreeTable(vm, &vm->strings);
    FalconFreeTable(vm, &vm->globals);
    FalconFreeObjects(vm);
}

/**
 * Pushes a value to the Falcon's virtual machine stack.
 */
bool FalconPush(FalconVM *vm, FalconValue value) {
    if ((vm->stackTop - &vm->stack[0]) > FALCON_STACK_MAX - 1) {
        FalconVMError(vm, FALCON_STACK_OVERFLOW);
        return false;
    }

    *vm->stackTop = value;
    vm->stackTop++;
    return true;
}

/**
 * Pops a value from the Falcon's virtual machine stack.
 */
FalconValue FalconPop(FalconVM *vm) {
    vm->stackTop--;
    return *vm->stackTop;
}

/**
 * Peeks a element on the Falcon's virtual machine stack.
 */
static FalconValue peek(FalconVM *vm, int distance) { return vm->stackTop[-1 - distance]; }

/**
 * Executes a call on the given Falcon function by setting its call frame to be run.
 */
static bool call(FalconVM *vm, FalconObjClosure *closure, int argCount) {
    if (argCount != closure->function->arity) {
        FalconVMError(vm, FALCON_ARGS_COUNT_ERR, closure->function->arity, argCount);
        return false;
    }

    if (vm->frameCount == FALCON_FRAMES_MAX) {
        FalconVMError(vm, FALCON_STACK_OVERFLOW);
        return false;
    }

    FalconCallFrame *frame = &vm->frames[vm->frameCount++];
    frame->closure = closure;
    frame->pc = closure->function->bytecode.code;
    frame->slots = vm->stackTop - argCount - 1;
    return true;
}

/**
 * Tries to execute a function call on a given Falcon value.
 */
static bool callValue(FalconVM *vm, FalconValue callee, int argCount) {
    if (FALCON_IS_OBJ(callee)) {
        switch (FALCON_OBJ_TYPE(callee)) {
            case FALCON_OBJ_CLOSURE:
                return call(vm, FALCON_AS_CLOSURE(callee), argCount);
            case FALCON_OBJ_NATIVE: {
                FalconNativeFn native = FALCON_AS_NATIVE(callee)->function;
                FalconValue out =
                    native(vm, argCount, vm->stackTop - argCount); /* Runs native func */
                if (FALCON_IS_ERR(out)) return false; /* Checks if a runtime error occurred */
                vm->stackTop -= argCount + 1;         /* Update the stack to before function call */
                if (!FalconPush(vm, out)) return false; /* Pushes the return value on the stack */
                return true;
            }
            default:
                break; /* Not callable */
        }
    }

    FalconVMError(vm, FALCON_VALUE_NOT_CALL_ERR);
    return false;
}

/**
 * Captures a given local upvalue.
 */
static FalconObjUpvalue *captureUpvalue(FalconVM *vm, FalconValue *local) {
    FalconObjUpvalue *prevUpvalue = NULL;
    FalconObjUpvalue *upvalue = vm->openUpvalues;

    /* Iterate past upvalues pointing to slots above the given one */
    while (upvalue != NULL && upvalue->slot > local) {
        prevUpvalue = upvalue;
        upvalue = upvalue->next;
    }

    if (upvalue != NULL && upvalue->slot == local) /* Checks if already exists in the list */
        return upvalue;

    FalconObjUpvalue *createdUpvalue = FalconNewUpvalue(vm, local); /* Creates a new upvalue */
    createdUpvalue->next = upvalue;                                 /* Adds to the list */

    if (prevUpvalue == NULL) {
        vm->openUpvalues = createdUpvalue;
    } else {
        prevUpvalue->next = createdUpvalue;
    }

    return createdUpvalue;
}

/**
 * Closes the upvalues for a given stack slot.
 */
static void closeUpvalues(FalconVM *vm, FalconValue *last) {
    while (vm->openUpvalues != NULL && vm->openUpvalues->slot >= last) {
        FalconObjUpvalue *upvalue = vm->openUpvalues;
        upvalue->closed = *upvalue->slot;
        upvalue->slot = &upvalue->closed;
        vm->openUpvalues = upvalue->next;
    }
}

/**
 * Compares the two string values on the top of the virtual machine stack.
 */
static int compareStrings(FalconVM *vm) {
    FalconObjString *str2 = FALCON_AS_STRING(FalconPop(vm));
    return FalconCompareStrings(FALCON_AS_STRING(vm->stackTop[-1]), str2);
}

/**
 * Concatenates the two string values on the top of the virtual machine stack. Then, pushes the new
 * string to the stack.
 */
static void concatenateStrings(FalconVM *vm) {
    FalconObjString *b = FALCON_AS_STRING(peek(vm, 0));
    FalconObjString *a = FALCON_AS_STRING(peek(vm, 1));
    FalconObjString *result = FalconConcatStrings(vm, b, a);   /* Concatenates both strings */
    FalconPop(vm);                                             /* Pops string "b" */
    FalconPop(vm);                                             /* Pops string "a" */
    FalconPush(vm, FALCON_OBJ_VAL(result));                    /* Pushes string "result" */
    FalconTableSet(vm, &vm->strings, result, FALCON_NULL_VAL); /* Interns the string */
}

/**
 * Loops through all the instructions in a bytecode chunk. Each turn through the loop, it reads and
 * executes the current bytecode instruction.
 */
static FalconResultCode run(FalconVM *vm) {
    FalconCallFrame *frame = &vm->frames[vm->frameCount - 1]; /* Current call frame */

/* Constants of the current running bytecode */
#define FALCON_CURR_CONSTANTS() frame->closure->function->bytecode.constants

/* Reads the next 8 bits (byte) or 16 bits (2 bytes) */
#define FALCON_READ_BYTE()  (*frame->pc++)
#define FALCON_READ_SHORT() (frame->pc += 2, ((uint16_t) (frame->pc[-2] << 8u) | frame->pc[-1]))

/* Reads the next byte from the bytecode, treats the resulting number as an index, and looks up the
 * corresponding location in the chunk’s constant table */
#define FALCON_READ_CONSTANT() FALCON_CURR_CONSTANTS().values[FALCON_READ_BYTE()]
#define FALCON_READ_STRING()   FALCON_AS_STRING(FALCON_READ_CONSTANT())

/* Checks if the two elements at the top of the Falcon VM's stack are numerical Values. If not, a
 * runtime error is returned */
#define FALCON_ASSERT_TOP2_NUM(vm)                                        \
    do {                                                                  \
        if (!FALCON_IS_NUM(peek(vm, 0)) || !FALCON_IS_NUM(peek(vm, 1))) { \
            FalconVMError(vm, FALCON_OPR_NOT_NUM_ERR);                    \
            return FALCON_RUNTIME_ERROR;                                  \
        }                                                                 \
    } while (false)

/* Checks if the element at the top of the Falcon VM's stack is a numerical Value. If not, a
 * runtime error is returned */
#define FALCON_ASSERT_TOP_NUM(vm)                      \
    do {                                               \
        if (!FALCON_IS_NUM(peek(vm, 0))) {             \
            FalconVMError(vm, FALCON_OPR_NOT_NUM_ERR); \
            return FALCON_RUNTIME_ERROR;               \
        }                                              \
    } while (false)

/* Checks if the element at the top of the Falcon VM's stack is not zero. If not, a runtime error
 * is returned */
#define FALCON_ASSERT_TOP_NOT_0(vm)                 \
    do {                                            \
        if (FALCON_AS_NUM(peek(vm, 0)) == 0) {      \
            FalconVMError(vm, FALCON_DIV_ZERO_ERR); \
            return FALCON_RUNTIME_ERROR;            \
        }                                           \
    } while (false)

/* Performs a binary operation of the 'op' operator on the two elements on the top of the Falcon
 * VM's stack. Then, sets the result on the top of the stack */
#define FALCON_BINARY_OP(vm, op, valueType)           \
    do {                                              \
        FALCON_ASSERT_TOP2_NUM(vm);                   \
        double b = FALCON_AS_NUM(FalconPop(vm));      \
        double a = FALCON_AS_NUM((vm)->stackTop[-1]); \
        (vm)->stackTop[-1] = valueType(a op b);       \
    } while (false)

/* Performs a division operation (integer mod or double division) on the two elements on the top of
 * the Falcon VM's stack. Then, sets the result on the top of the stack */
#define FALCON_DIVISION_OP(vm, op, type)             \
    do {                                             \
        FALCON_ASSERT_TOP2_NUM(vm);                  \
        FALCON_ASSERT_TOP_NOT_0(vm);                 \
        type b = FALCON_AS_NUM(FalconPop(vm));       \
        type a = FALCON_AS_NUM((vm)->stackTop[-1]);  \
        (vm)->stackTop[-1] = FALCON_NUM_VAL(a op b); \
    } while (false)

/* Performs a greater/less (GL) comparison operation of the 'op' operator on the two elements on the
 * top of the Falcon VM's stack. Then, sets the result on the top of the stack */
#define FALCON_GL_COMPARE(vm, op)                                                         \
    do {                                                                                  \
        if (FALCON_IS_STRING(peek(vm, 0)) && FALCON_IS_STRING(peek(vm, 1))) {             \
            int comparison = compareStrings(vm);                                          \
            (vm)->stackTop[-1] =                                                          \
                (comparison op 0) ? FALCON_BOOL_VAL(true) : FALCON_BOOL_VAL(false);       \
        } else if (FALCON_IS_NUM(peek(vm, 0)) && FALCON_IS_NUM(peek(vm, 1))) {            \
            double a = FALCON_AS_NUM(FalconPop(vm));                                      \
            (vm)->stackTop[-1] = FALCON_BOOL_VAL(FALCON_AS_NUM((vm)->stackTop[-1]) op a); \
        } else {                                                                          \
            FalconVMError(vm, FALCON_OPR_NOT_NUM_STR_ERR);                                \
            return FALCON_RUNTIME_ERROR;                                                  \
        }                                                                                 \
    } while (false)

#ifdef FALCON_DEBUG_TRACE_EXECUTION
    FalconExecutionHeader();
#endif

    while (true) {
#ifdef FALCON_DEBUG_TRACE_EXECUTION
        if (vm->stack != vm->stackTop) FalconDumpStack(vm);
        FalconDumpInstruction(&frame->closure->function->bytecode,
                              (int) (frame->pc - frame->closure->function->bytecode.code));
#endif

        uint8_t instruction = FALCON_READ_BYTE();
        switch (instruction) { /* Reads the next byte and switches through the opcodes */

            /* Constants and literals */
            case FALCON_OP_CONSTANT: {
                uint16_t index = FALCON_READ_BYTE() | (uint16_t)(FALCON_READ_BYTE() << 8u);
                if (!FalconPush(vm, FALCON_CURR_CONSTANTS().values[index]))
                    return FALCON_RUNTIME_ERROR;
                break;
            }
            case FALCON_OP_FALSE:
                if (!FalconPush(vm, FALCON_BOOL_VAL(false))) return FALCON_RUNTIME_ERROR;
                break;
            case FALCON_OP_TRUE:
                if (!FalconPush(vm, FALCON_BOOL_VAL(true))) return FALCON_RUNTIME_ERROR;
                break;
            case FALCON_OP_NULL:
                if (!FalconPush(vm, FALCON_NULL_VAL)) return FALCON_RUNTIME_ERROR;
                break;

            /* Relational operations */
            case FALCON_OP_AND: {
                uint16_t offset = FALCON_READ_SHORT();
                if (FalconIsFalsey(peek(vm, 0)))
                    frame->pc += offset;
                else
                    FalconPop(vm);
                break;
            }
            case FALCON_OP_OR: {
                uint16_t offset = FALCON_READ_SHORT();
                if (FalconIsFalsey(peek(vm, 0)))
                    FalconPop(vm);
                else
                    frame->pc += offset;
                break;
            }
            case FALCON_OP_NOT:
                vm->stackTop[-1] = FALCON_BOOL_VAL(FalconIsFalsey(vm->stackTop[-1]));
                break;
            case FALCON_OP_EQUAL: {
                FalconValue b = FalconPop(vm);
                vm->stackTop[-1] = FALCON_BOOL_VAL(FalconValuesEqual(vm->stackTop[-1], b));
                break;
            }
            case FALCON_OP_GREATER:
                FALCON_GL_COMPARE(vm, >);
                break;
            case FALCON_OP_LESS:
                FALCON_GL_COMPARE(vm, <);
                break;

            /* Arithmetic operations */
            case FALCON_OP_ADD: {
                if (FALCON_IS_STRING(peek(vm, 0)) && FALCON_IS_STRING(peek(vm, 1))) {
                    concatenateStrings(vm);
                } else if (FALCON_IS_NUM(peek(vm, 0)) && FALCON_IS_NUM(peek(vm, 1))) {
                    double a = FALCON_AS_NUM(FalconPop(vm));
                    vm->stackTop[-1] = FALCON_NUM_VAL(FALCON_AS_NUM(vm->stackTop[-1]) + a);
                } else {
                    FalconVMError(vm, FALCON_OPR_NOT_NUM_STR_ERR);
                    return FALCON_RUNTIME_ERROR;
                }
                break;
            }
            case FALCON_OP_SUBTRACT:
                FALCON_BINARY_OP(vm, -, FALCON_NUM_VAL);
                break;
            case FALCON_OP_NEGATE:
                FALCON_ASSERT_TOP_NUM(vm);
                vm->stackTop[-1] = FALCON_NUM_VAL(-FALCON_AS_NUM(vm->stackTop[-1]));
                break;
            case FALCON_OP_MULTIPLY:
                FALCON_BINARY_OP(vm, *, FALCON_NUM_VAL);
                break;
            case FALCON_OP_MOD:
                FALCON_DIVISION_OP(vm, %, int);
                break;
            case FALCON_OP_DIVIDE: {
                FALCON_DIVISION_OP(vm, /, double);
                break;
            }
            case FALCON_OP_POW: {
                FALCON_ASSERT_TOP2_NUM(vm);
                double a = FALCON_AS_NUM(FalconPop(vm));
                vm->stackTop[-1] = FALCON_NUM_VAL(FalconPow(FALCON_AS_NUM(vm->stackTop[-1]), a));
                break;
            }

            /* Variable operations */
            case FALCON_OP_DEFINE_GLOBAL: {
                FalconObjString *name = FALCON_READ_STRING();
                FalconTableSet(vm, &vm->globals, name, peek(vm, 0));
                FalconPop(vm);
                break;
            }
            case FALCON_OP_GET_GLOBAL: {
                FalconObjString *name = FALCON_READ_STRING();
                FalconValue value;
                if (!FalconTableGet(&vm->globals, name, &value)) /* Checks if undefined */
                    return undefinedVariableError(vm, name, false);
                if (!FalconPush(vm, value)) return FALCON_RUNTIME_ERROR;
                break;
            }
            case FALCON_OP_SET_GLOBAL: {
                FalconObjString *name = FALCON_READ_STRING();
                if (FalconTableSet(vm, &vm->globals, name, peek(vm, 0))) /* Checks if undefined */
                    return undefinedVariableError(vm, name, true);
                break;
            }
            case FALCON_OP_GET_UPVALUE: {
                uint8_t slot = FALCON_READ_BYTE();
                if (!FalconPush(vm, *frame->closure->upvalues[slot]->slot))
                    return FALCON_RUNTIME_ERROR;
                break;
            }
            case FALCON_OP_SET_UPVALUE: {
                uint8_t slot = FALCON_READ_BYTE();
                *frame->closure->upvalues[slot]->slot = peek(vm, 0);
                break;
            }
            case FALCON_OP_CLOSE_UPVALUE:
                closeUpvalues(vm, vm->stackTop - 1);
                FalconPop(vm);
                break;
            case FALCON_OP_GET_LOCAL: {
                uint8_t slot = FALCON_READ_BYTE();
                if (!FalconPush(vm, frame->slots[slot])) return FALCON_RUNTIME_ERROR;
                break;
            }
            case FALCON_OP_SET_LOCAL: {
                uint8_t slot = FALCON_READ_BYTE();
                frame->slots[slot] = peek(vm, 0);
                break;
            }

            /* Jump/loop operations */
            case FALCON_OP_JUMP: {
                uint16_t offset = FALCON_READ_SHORT();
                frame->pc += offset;
                break;
            }
            case FALCON_OP_JUMP_IF_FALSE: {
                uint16_t offset = FALCON_READ_SHORT();
                if (FalconIsFalsey(peek(vm, 0))) frame->pc += offset;
                break;
            }
            case FALCON_OP_LOOP: {
                uint16_t offset = FALCON_READ_SHORT();
                frame->pc -= offset;
                break;
            }

            /* Function operations */
            case FALCON_OP_CLOSURE: {
                FalconObjFunction *function = FALCON_AS_FUNCTION(FALCON_READ_CONSTANT());
                FalconObjClosure *closure = FalconNewClosure(vm, function);
                if (!FalconPush(vm, FALCON_OBJ_VAL(closure))) return FALCON_RUNTIME_ERROR;

                /* Capture upvalues */
                for (int i = 0; i < closure->upvalueCount; i++) {
                    uint8_t isLocal = FALCON_READ_BYTE();
                    uint8_t index = FALCON_READ_BYTE();

                    if (isLocal) {
                        closure->upvalues[i] = captureUpvalue(vm, frame->slots + index);
                    } else {
                        closure->upvalues[i] = frame->closure->upvalues[index];
                    }
                }

                break;
            }
            case FALCON_OP_CALL: {
                int argCount = FALCON_READ_BYTE();
                if (!callValue(vm, peek(vm, argCount), argCount)) return FALCON_RUNTIME_ERROR;
                frame = &vm->frames[vm->frameCount - 1]; /* Updates the current frame */
                break;
            }
            case FALCON_OP_RETURN: {
                FalconValue result = FalconPop(vm); /* Gets the function's return value */
                closeUpvalues(vm, frame->slots);    /* Closes upvalues */
                vm->frameCount--;

                if (vm->frameCount == 0) { /* Checks if top level code is finished */
                    FalconPop(vm);         /* Pops "script" from the stack */
                    return FALCON_OK;
                }

                vm->stackTop = frame->slots; /* Resets the stack top */
                if (!FalconPush(vm, result))
                    return FALCON_RUNTIME_ERROR;         /* Pushes the return value */
                frame = &vm->frames[vm->frameCount - 1]; /* Updates the current frame */
                break;
            }

            /* VM operations */
            case FALCON_OP_DUP:
                FalconPush(vm, peek(vm, 0));
                break;
            case FALCON_OP_POP:
                FalconPop(vm);
                break;
            case FALCON_OP_POP_EXPR: {
                FalconValue result = FalconPop(vm);
                bool isString = FALCON_IS_STRING(result);
                printf(" => ");
                if (isString) printf("\"");
                FalconPrintValue(result);
                if (isString) printf("\"");
                printf("\n");
                break;
            }
            case FALCON_OP_TEMP:
                FalconVMError(vm, FALCON_UNREACHABLE_ERR, instruction);
                return FALCON_RUNTIME_ERROR;

            /* Unknown opcode */
            default:
                FalconVMError(vm, FALCON_UNKNOWN_OPCODE_ERR, instruction);
                return FALCON_RUNTIME_ERROR;
        }
    }

#undef FALCON_READ_BYTE
#undef FALCON_READ_SHORT
#undef FALCON_READ_CONSTANT
#undef FALCON_READ_STRING
#undef FALCON_ASSERT_TOP2_NUM
#undef FALCON_ASSERT_TOP_NUM
#undef FALCON_ASSERT_TOP_NOT_0
#undef FALCON_BINARY_OP
#undef FALCON_DIVISION_OP
#undef FALCON_GL_COMPARE
}

/**
 * Interprets a Falcon's source code string. If the source is compiled successfully, the bytecode
 * chunk is set for the Falcon's virtual machine to execute.
 */
FalconResultCode FalconInterpret(FalconVM *vm, const char *source) {
    FalconObjFunction *function = FalconCompile(vm, source); /* Compiles the source code */
    if (function == NULL) return FALCON_COMPILE_ERROR;

    /* Set the script to run */
    FalconPush(vm, FALCON_OBJ_VAL(function));
    FalconObjClosure *closure = FalconNewClosure(vm, function);
    FalconPop(vm);
    FalconPush(vm, FALCON_OBJ_VAL(closure));
    callValue(vm, FALCON_OBJ_VAL(closure), 0);

    return run(vm); /* Executes the bytecode chunk */
}
