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

#ifdef FALCON_DEBUG_LEVEL_01
#include "falcon_debug.h"
#endif

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
void falconVMError(FalconVM *vm, const char *format, ...) {
    va_list args;
    va_start(args, format);
    falconRuntimeError(vm, format, args); /* Presents the error */
    va_end(args);
    resetVMStack(vm); /* Resets the stack due to error */
}

/**
 * Presents a runtime error when a global variable is undefined.
 */
static FalconResultCode undefinedVariableError(FalconVM *vm, ObjString *name, bool delete) {
    if (delete) falconTableDelete(&vm->globals, name);
    falconVMError(vm, FALCON_UNDEF_VAR_ERR, name->chars);
    return FALCON_RUNTIME_ERROR;
}

/**
 * Initializes the Falcon's virtual machine.
 */
void falconInitVM(FalconVM *vm) {
    resetVMStack(vm); /* Inits the VM stack */

    /* Inits the VM fields */
    vm->fileName = NULL;
    vm->isREPL = false;
    vm->objects = NULL;
    vm->compiler = NULL;

    /* Inits the garbage collection fields */
    vm->grayCount = 0;
    vm->grayCapacity = 0;
    vm->grayStack = NULL;
    vm->bytesAllocated = 0;
    vm->nextGC = FALCON_BASE_HEAP_SIZE;

    falconInitTable(&vm->strings); /* Inits the table of interned strings */
    falconInitTable(&vm->globals); /* Inits the table of globals */
    falconDefNatives(vm);          /* Sets native functions */
}

/**
 * Frees the Falcon's virtual machine and its allocated objects.
 */
void falconFreeVM(FalconVM *vm) {
    falconFreeTable(vm, &vm->strings);
    falconFreeTable(vm, &vm->globals);
    falconFreeObjs(vm);
}

/**
 * Pushes a value to the Falcon's virtual machine stack.
 */
bool falconPush(FalconVM *vm, FalconValue value) {
    if ((vm->stackTop - &vm->stack[0]) > FALCON_STACK_MAX - 1) {
        falconVMError(vm, FALCON_STACK_OVERFLOW);
        return false;
    }

    *vm->stackTop = value;
    vm->stackTop++;
    return true;
}

/**
 * Pops a value from the Falcon's virtual machine stack.
 */
FalconValue falconPop(FalconVM *vm) {
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
static bool call(FalconVM *vm, ObjClosure *closure, int argCount) {
    if (argCount != closure->function->arity) {
        falconVMError(vm, FALCON_ARGS_COUNT_ERR, closure->function->arity, argCount);
        return false;
    }

    if (vm->frameCount == FALCON_FRAMES_MAX) {
        falconVMError(vm, FALCON_STACK_OVERFLOW);
        return false;
    }

    CallFrame *frame = &vm->frames[vm->frameCount++];
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
            case OBJ_CLOSURE:
                return call(vm, FALCON_AS_CLOSURE(callee), argCount);
            case OBJ_NATIVE: {
                FalconNativeFn native = FALCON_AS_NATIVE(callee)->function;
                FalconValue out =
                    native(vm, argCount, vm->stackTop - argCount); /* Runs native func */
                if (FALCON_IS_ERR(out)) return false; /* Checks if a runtime error occurred */
                vm->stackTop -= argCount + 1;         /* Update the stack to before function call */
                if (!falconPush(vm, out)) return false; /* Pushes the return value on the stack */
                return true;
            }
            default:
                break; /* Not callable */
        }
    }

    falconVMError(vm, FALCON_VALUE_NOT_CALL_ERR);
    return false;
}

/**
 * Captures a given local upvalue.
 */
static ObjUpvalue *captureUpvalue(FalconVM *vm, FalconValue *local) {
    ObjUpvalue *prevUpvalue = NULL;
    ObjUpvalue *upvalue = vm->openUpvalues;

    /* Iterate past upvalues pointing to slots above the given one */
    while (upvalue != NULL && upvalue->slot > local) {
        prevUpvalue = upvalue;
        upvalue = upvalue->next;
    }

    if (upvalue != NULL && upvalue->slot == local) /* Checks if already exists in the list */
        return upvalue;

    ObjUpvalue *createdUpvalue = falconUpvalue(vm, local); /* Creates a new upvalue */
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
        ObjUpvalue *upvalue = vm->openUpvalues;
        upvalue->closed = *upvalue->slot;
        upvalue->slot = &upvalue->closed;
        vm->openUpvalues = upvalue->next;
    }
}

/**
 * Compares the two string values on the top of the virtual machine stack.
 */
static int compareStrings(FalconVM *vm) {
    ObjString *str2 = FALCON_AS_STRING(falconPop(vm));
    return falconCompareStrings(FALCON_AS_STRING(vm->stackTop[-1]), str2);
}

/**
 * Concatenates the two string values on the top of the virtual machine stack. Then, pushes the new
 * string to the stack.
 */
static void concatenateStrings(FalconVM *vm) {
    ObjString *b = FALCON_AS_STRING(peek(vm, 0));
    ObjString *a = FALCON_AS_STRING(peek(vm, 1));
    ObjString *result = falconConcatStrings(vm, b, a);   /* Concatenates both strings */
    falconPop(vm);                                             /* Pops string "b" */
    falconPop(vm);                                             /* Pops string "a" */
    falconPush(vm, FALCON_OBJ_VAL(result));                    /* Pushes string "result" */
    falconTableSet(vm, &vm->strings, result, FALCON_NULL_VAL); /* Interns the string */
}

/**
 * Loops through all the instructions in a bytecode chunk. Each turn through the loop, it reads and
 * executes the current bytecode instruction.
 */
static FalconResultCode run(FalconVM *vm) {
    CallFrame *frame = &vm->frames[vm->frameCount - 1]; /* Current call frame */

/* Constants of the current running bytecode */
#define CURR_CONSTANTS() frame->closure->function->bytecode.constants

/* Reads the next 8 bits (byte) or 16 bits (2 bytes) */
#define READ_BYTE()  (*frame->pc++)
#define READ_SHORT() (frame->pc += 2, ((uint16_t) (frame->pc[-2] << 8u) | frame->pc[-1]))

/* Reads the next byte from the bytecode, treats the resulting number as an index, and looks up the
 * corresponding location in the chunk’s constant table */
#define READ_CONSTANT() CURR_CONSTANTS().values[READ_BYTE()]
#define READ_STRING()   FALCON_AS_STRING(READ_CONSTANT())

/* Checks if the two elements at the top of the Falcon VM's stack are numerical Values. If not, a
 * runtime error is returned */
#define ASSERT_TOP2_NUM(vm)                                               \
    do {                                                                  \
        if (!FALCON_IS_NUM(peek(vm, 0)) || !FALCON_IS_NUM(peek(vm, 1))) { \
            falconVMError(vm, FALCON_OPR_NOT_NUM_ERR);                    \
            return FALCON_RUNTIME_ERROR;                                  \
        }                                                                 \
    } while (false)

/* Checks if the element at the top of the Falcon VM's stack is a numerical Value. If not, a
 * runtime error is returned */
#define ASSERT_TOP_NUM(vm)                             \
    do {                                               \
        if (!FALCON_IS_NUM(peek(vm, 0))) {             \
            falconVMError(vm, FALCON_OPR_NOT_NUM_ERR); \
            return FALCON_RUNTIME_ERROR;               \
        }                                              \
    } while (false)

/* Checks if the element at the top of the Falcon VM's stack is not zero. If not, a runtime error
 * is returned */
#define ASSERT_TOP_NOT_0(vm)                        \
    do {                                            \
        if (FALCON_AS_NUM(peek(vm, 0)) == 0) {      \
            falconVMError(vm, FALCON_DIV_ZERO_ERR); \
            return FALCON_RUNTIME_ERROR;            \
        }                                           \
    } while (false)

/* Performs a binary operation of the 'op' operator on the two elements on the top of the Falcon
 * VM's stack. Then, sets the result on the top of the stack */
#define BINARY_OP(vm, op, valueType)                  \
    do {                                              \
        ASSERT_TOP2_NUM(vm);                          \
        double b = FALCON_AS_NUM(falconPop(vm));      \
        double a = FALCON_AS_NUM((vm)->stackTop[-1]); \
        (vm)->stackTop[-1] = valueType(a op b);       \
    } while (false)

/* Performs a division operation (integer mod or double division) on the two elements on the top of
 * the Falcon VM's stack. Then, sets the result on the top of the stack */
#define DIVISION_OP(vm, op, type)                    \
    do {                                             \
        ASSERT_TOP2_NUM(vm);                         \
        ASSERT_TOP_NOT_0(vm);                        \
        type b = FALCON_AS_NUM(falconPop(vm));       \
        type a = FALCON_AS_NUM((vm)->stackTop[-1]);  \
        (vm)->stackTop[-1] = FALCON_NUM_VAL(a op b); \
    } while (false)

/* Performs a greater/less (GL) comparison operation of the 'op' operator on the two elements on the
 * top of the Falcon VM's stack. Then, sets the result on the top of the stack */
#define GL_COMPARE(vm, op)                                                                \
    do {                                                                                  \
        if (FALCON_IS_STRING(peek(vm, 0)) && FALCON_IS_STRING(peek(vm, 1))) {             \
            int comparison = compareStrings(vm);                                          \
            (vm)->stackTop[-1] =                                                          \
                (comparison op 0) ? FALCON_BOOL_VAL(true) : FALCON_BOOL_VAL(false);       \
        } else if (FALCON_IS_NUM(peek(vm, 0)) && FALCON_IS_NUM(peek(vm, 1))) {            \
            double a = FALCON_AS_NUM(falconPop(vm));                                      \
            (vm)->stackTop[-1] = FALCON_BOOL_VAL(FALCON_AS_NUM((vm)->stackTop[-1]) op a); \
        } else {                                                                          \
            falconVMError(vm, FALCON_OPR_NOT_NUM_STR_ERR);                                \
            return FALCON_RUNTIME_ERROR;                                                  \
        }                                                                                 \
    } while (false)

    while (true) {
#ifdef FALCON_DEBUG_LEVEL_01
        if (vm->stack != vm->stackTop) falconDumpStack(vm);
        falconDumpInstruction(vm, &frame->closure->function->bytecode,
                              (int) (frame->pc - frame->closure->function->bytecode.code));
#endif

        uint8_t instruction = READ_BYTE();
        switch (instruction) { /* Reads the next byte and switches through the opcodes */

            /* Constants and literals */
            case OP_CONSTANT: {
                uint16_t index = READ_BYTE() | (uint16_t)(READ_BYTE() << 8u);
                if (!falconPush(vm, CURR_CONSTANTS().values[index]))
                    return FALCON_RUNTIME_ERROR;
                break;
            }
            case OP_FALSE_LIT:
                if (!falconPush(vm, FALCON_BOOL_VAL(false))) return FALCON_RUNTIME_ERROR;
                break;
            case OP_TRUE_LIT:
                if (!falconPush(vm, FALCON_BOOL_VAL(true))) return FALCON_RUNTIME_ERROR;
                break;
            case OP_NULL_LIT:
                if (!falconPush(vm, FALCON_NULL_VAL)) return FALCON_RUNTIME_ERROR;
                break;

            /* Relational operations */
            case OP_AND: {
                uint16_t offset = READ_SHORT();
                if (falconIsFalsey(peek(vm, 0)))
                    frame->pc += offset;
                else
                    falconPop(vm);
                break;
            }
            case OP_OR: {
                uint16_t offset = READ_SHORT();
                if (falconIsFalsey(peek(vm, 0)))
                    falconPop(vm);
                else
                    frame->pc += offset;
                break;
            }
            case OP_NOT:
                vm->stackTop[-1] = FALCON_BOOL_VAL(falconIsFalsey(vm->stackTop[-1]));
                break;
            case OP_EQUAL: {
                FalconValue b = falconPop(vm);
                vm->stackTop[-1] = FALCON_BOOL_VAL(falconValEqual(vm->stackTop[-1], b));
                break;
            }
            case OP_GREATER:
                GL_COMPARE(vm, >);
                break;
            case OP_LESS:
                GL_COMPARE(vm, <);
                break;

            /* Arithmetic operations */
            case OP_ADD: {
                if (FALCON_IS_STRING(peek(vm, 0)) && FALCON_IS_STRING(peek(vm, 1))) {
                    concatenateStrings(vm);
                } else if (FALCON_IS_NUM(peek(vm, 0)) && FALCON_IS_NUM(peek(vm, 1))) {
                    double a = FALCON_AS_NUM(falconPop(vm));
                    vm->stackTop[-1] = FALCON_NUM_VAL(FALCON_AS_NUM(vm->stackTop[-1]) + a);
                } else {
                    falconVMError(vm, FALCON_OPR_NOT_NUM_STR_ERR);
                    return FALCON_RUNTIME_ERROR;
                }
                break;
            }
            case OP_SUBTRACT:
                BINARY_OP(vm, -, FALCON_NUM_VAL);
                break;
            case OP_NEGATE:
                ASSERT_TOP_NUM(vm);
                vm->stackTop[-1] = FALCON_NUM_VAL(-FALCON_AS_NUM(vm->stackTop[-1]));
                break;
            case OP_MULTIPLY:
                BINARY_OP(vm, *, FALCON_NUM_VAL);
                break;
            case OP_MOD:
                DIVISION_OP(vm, %, int);
                break;
            case OP_DIVIDE: {
                DIVISION_OP(vm, /, double);
                break;
            }
            case OP_POW: {
                ASSERT_TOP2_NUM(vm);
                double a = FALCON_AS_NUM(falconPop(vm));
                vm->stackTop[-1] = FALCON_NUM_VAL(falconPow(FALCON_AS_NUM(vm->stackTop[-1]), a));
                break;
            }

            /* Variable operations */
            case OP_DEFINE_GLOBAL: {
                ObjString *name = READ_STRING();
                falconTableSet(vm, &vm->globals, name, peek(vm, 0));
                falconPop(vm);
                break;
            }
            case OP_GET_GLOBAL: {
                ObjString *name = READ_STRING();
                FalconValue value;
                if (!falconTableGet(&vm->globals, name, &value)) /* Checks if undefined */
                    return undefinedVariableError(vm, name, false);
                if (!falconPush(vm, value)) return FALCON_RUNTIME_ERROR;
                break;
            }
            case OP_SET_GLOBAL: {
                ObjString *name = READ_STRING();
                if (falconTableSet(vm, &vm->globals, name, peek(vm, 0))) /* Checks if undefined */
                    return undefinedVariableError(vm, name, true);
                break;
            }
            case OP_GET_UPVALUE: {
                uint8_t slot = READ_BYTE();
                if (!falconPush(vm, *frame->closure->upvalues[slot]->slot))
                    return FALCON_RUNTIME_ERROR;
                break;
            }
            case OP_SET_UPVALUE: {
                uint8_t slot = READ_BYTE();
                *frame->closure->upvalues[slot]->slot = peek(vm, 0);
                break;
            }
            case OP_CLOSE_UPVALUE:
                closeUpvalues(vm, vm->stackTop - 1);
                falconPop(vm);
                break;
            case OP_GET_LOCAL: {
                uint8_t slot = READ_BYTE();
                if (!falconPush(vm, frame->slots[slot])) return FALCON_RUNTIME_ERROR;
                break;
            }
            case OP_SET_LOCAL: {
                uint8_t slot = READ_BYTE();
                frame->slots[slot] = peek(vm, 0);
                break;
            }

            /* Jump/loop operations */
            case OP_JUMP: {
                uint16_t offset = READ_SHORT();
                frame->pc += offset;
                break;
            }
            case OP_JUMP_IF_FALSE: {
                uint16_t offset = READ_SHORT();
                if (falconIsFalsey(peek(vm, 0))) frame->pc += offset;
                break;
            }
            case OP_LOOP: {
                uint16_t offset = READ_SHORT();
                frame->pc -= offset;
                break;
            }

            /* Function operations */
            case OP_CLOSURE: {
                ObjFunction *function = FALCON_AS_FUNCTION(READ_CONSTANT());
                ObjClosure *closure = falconClosure(vm, function);
                if (!falconPush(vm, FALCON_OBJ_VAL(closure))) return FALCON_RUNTIME_ERROR;

                /* Capture upvalues */
                for (int i = 0; i < closure->upvalueCount; i++) {
                    uint8_t isLocal = READ_BYTE();
                    uint8_t index = READ_BYTE();

                    if (isLocal) {
                        closure->upvalues[i] = captureUpvalue(vm, frame->slots + index);
                    } else {
                        closure->upvalues[i] = frame->closure->upvalues[index];
                    }
                }

                break;
            }
            case OP_CALL: {
                int argCount = READ_BYTE();
                if (!callValue(vm, peek(vm, argCount), argCount)) return FALCON_RUNTIME_ERROR;
                frame = &vm->frames[vm->frameCount - 1]; /* Updates the current frame */
                break;
            }
            case OP_RETURN: {
                FalconValue result = falconPop(vm); /* Gets the function's return value */
                closeUpvalues(vm, frame->slots);    /* Closes upvalues */
                vm->frameCount--;

                if (vm->frameCount == 0) { /* Checks if top level code is finished */
                    falconPop(vm);         /* Pops "script" from the stack */
                    return FALCON_OK;
                }

                vm->stackTop = frame->slots; /* Resets the stack top */
                if (!falconPush(vm, result))
                    return FALCON_RUNTIME_ERROR;         /* Pushes the return value */
                frame = &vm->frames[vm->frameCount - 1]; /* Updates the current frame */
                break;
            }

            /* VM operations */
            case OP_DUP:
                falconPush(vm, peek(vm, 0));
                break;
            case OP_POP:
                falconPop(vm);
                break;
            case OP_POP_EXPR: {
                FalconValue result = peek(vm, 0);
                if (!FALCON_IS_NULL(result)) {
                    bool isString = FALCON_IS_STRING(result);
                    if (isString) printf("\"");
                    falconPrintVal(vm, result);
                    if (isString) printf("\"");
                    printf("\n");
                }

                falconPop(vm);
                break;
            }
            case OP_TEMP:
                falconVMError(vm, FALCON_UNREACHABLE_ERR, instruction);
                return FALCON_RUNTIME_ERROR;

            /* Unknown opcode */
            default:
                falconVMError(vm, FALCON_UNKNOWN_OPCODE_ERR, instruction);
                return FALCON_RUNTIME_ERROR;
        }
    }

#undef READ_BYTE
#undef READ_SHORT
#undef READ_CONSTANT
#undef READ_STRING
#undef ASSERT_TOP2_NUM
#undef ASSERT_TOP_NUM
#undef ASSERT_TOP_NOT_0
#undef BINARY_OP
#undef DIVISION_OP
#undef GL_COMPARE
}

/**
 * Interprets a Falcon's source code string. If the source is compiled successfully, the bytecode
 * chunk is set for the Falcon's virtual machine to execute.
 */
FalconResultCode falconInterpret(FalconVM *vm, const char *source) {
    ObjFunction *function = falconCompile(vm, source); /* Compiles the source code */
    if (function == NULL) return FALCON_COMPILE_ERROR;

    /* Set the script to run */
    falconPush(vm, FALCON_OBJ_VAL(function));
    ObjClosure *closure = falconClosure(vm, function);
    falconPop(vm);
    falconPush(vm, FALCON_OBJ_VAL(closure));
    callValue(vm, FALCON_OBJ_VAL(closure), 0);

    return run(vm); /* Executes the bytecode chunk */
}
