/*
 * YAPL version 0.0.1 (master, Oct 18 2019)
 * yapl_vm.c: YAPL's stack-based virtual machine
 * See YAPL's license in the LICENSE file
 */

#include "yapl_vm.h"
#include "../compiler/yapl_compiler.h"
#include "../lib/math/yapl_math.h"
#include "../lib/string/yapl_string.h"
#include "../lib/yapl_error.h"
#include "../lib/yapl_natives.h"
#include "yapl_memmanager.h"
#include <stdarg.h>
#include <stdio.h>

#ifdef YAPL_DEBUG_TRACE_EXECUTION
#include "../lib/yapl_debug.h"
#endif

/**
 * Resets the virtual machine stack.
 */
static void resetVMStack(VM *vm) {
    vm->stackTop = vm->stack;
    vm->openUpvalues = NULL;
    vm->frameCount = 0;
}

/**
 * Presents a runtime error to the programmer and resets the VM stack.
 */
void VMError(VM *vm, const char *format, ...) {
    va_list args;
    va_start(args, format);
    runtimeError(vm, format, args); /* Presents the error */
    va_end(args);
    resetVMStack(vm); /* Resets the stack due to error */
}

/**
 * Presents a runtime error when a global variable is undefined.
 */
static ResultCode undefinedVariableError(VM *vm, ObjString *name, bool delete) {
    if (delete) tableDelete(&vm->globals, name);
    VMError(vm, UNDEF_VAR_ERR, name->chars);
    return RUNTIME_ERROR;
}

/**
 * Initializes the YAPL's virtual machine.
 */
void initVM(VM *vm) {
    resetVMStack(vm);
    initTable(&vm->strings);
    initTable(&vm->globals);
    vm->fileName = NULL;
    vm->isREPL = false;
    vm->objects = NULL;
    defineNatives(vm); /* Set native functions */
}

/**
 * Frees the YAPL's virtual machine and its allocated objects.
 */
void freeVM(VM *vm) {
    freeTable(&vm->strings);
    freeTable(&vm->globals);
    freeObjects(vm);
}

/**
 * Allocates a new YAPL upvalue object.
 */
ObjUpvalue *newUpvalue(VM *vm, Value *slot) {
    ObjUpvalue *upvalue = ALLOCATE_OBJ(vm, ObjUpvalue, OBJ_UPVALUE);
    upvalue->slot = slot;
    upvalue->next = NULL;
    upvalue->closed = NULL_VAL;
    return upvalue;
}

/**
 * Allocates a new YAPL closure object.
 */
ObjClosure *newClosure(VM *vm, ObjFunction *function) {
    ObjUpvalue **upvalues = ALLOCATE(ObjUpvalue *, function->upvalueCount); /* Sets upvalue list */

    for (int i = 0; i < function->upvalueCount; i++) {
        upvalues[i] = NULL; /* Initialize current upvalue */
    }

    ObjClosure *closure = ALLOCATE_OBJ(vm, ObjClosure, OBJ_CLOSURE);
    closure->function = function;
    closure->upvalues = upvalues;
    closure->upvalueCount = function->upvalueCount;
    return closure;
}

/**
 * Allocates a new YAPL function object.
 */
ObjFunction *newFunction(VM *vm) {
    ObjFunction *function = ALLOCATE_OBJ(vm, ObjFunction, OBJ_FUNCTION);
    function->arity = 0;
    function->upvalueCount = 0;
    function->name = NULL;
    initBytecodeChunk(&function->bytecodeChunk);
    return function;
}

/* Returns the count of elements in the VM stack */
#define STACK_COUNT(vm) (vm->stackTop - &vm->stack[0])

/**
 * Pushes a value to the YAPL's virtual machine stack.
 */
bool push(VM *vm, Value value) {
    if (STACK_COUNT(vm) > VM_STACK_MAX - 1) {
        VMError(vm, STACK_OVERFLOW);
        return false;
    }

    *vm->stackTop = value;
    vm->stackTop++;
    return true;
}

/**
 * Pops a value from the YAPL's virtual machine stack.
 */
Value pop(VM *vm) {
    vm->stackTop--;
    return *vm->stackTop;
}

/**
 * Peeks a element on the YAPL's virtual machine stack.
 */
static Value peek(VM *vm, int distance) { return vm->stackTop[-1 - distance]; }

/**
 * Prints the YAPL's virtual machine stack.
 */
void printStack(VM *vm) {
    printf("Stack: ");
    for (Value *slot = vm->stack; slot < vm->stackTop; slot++) {
        printf("[ ");
        printValue(*slot);
        printf(" ] ");
    }
    printf("\nStack count: %ld\n", STACK_COUNT(vm));
}

/**
 * Executes a call on the given YAPL function by setting its call frame to be run.
 */
static bool call(VM *vm, ObjClosure *closure, int argCount) {
    if (argCount != closure->function->arity) {
        VMError(vm, ARGS_COUNT_ERR, closure->function->arity, argCount);
        return false;
    }

    if (vm->frameCount == VM_FRAMES_MAX) {
        VMError(vm, STACK_OVERFLOW);
        return false;
    }

    CallFrame *frame = &vm->frames[vm->frameCount++];
    frame->closure = closure;
    frame->pc = closure->function->bytecodeChunk.code;
    frame->slots = vm->stackTop - argCount - 1;
    return true;
}

/**
 * Tries to execute a function call on a given YAPL value.
 */
static bool callValue(VM *vm, Value callee, int argCount) {
    if (IS_OBJ(callee)) {
        switch (OBJ_TYPE(callee)) {
            case OBJ_CLOSURE:
                return call(vm, AS_CLOSURE(callee), argCount);
            case OBJ_NATIVE: {
                NativeFn native = AS_NATIVE(callee);
                Value out = native(vm, argCount, vm->stackTop - argCount); /* Runs native func */
                if (IS_ERR(out)) return false;    /* Checks if a runtime error occurred */
                vm->stackTop -= argCount + 1;     /* Update the stack to before function call */
                if (!push(vm, out)) return false; /* Pushes the return value on the stack */
                return true;
            }
            default:
                break; /* Not callable */
        }
    }

    VMError(vm, VALUE_NOT_CALL_ERR);
    return false;
}

/**
 * Captures a given local upvalue.
 */
static ObjUpvalue *captureUpvalue(VM *vm, Value *local) {
    ObjUpvalue *prevUpvalue = NULL;
    ObjUpvalue *upvalue = vm->openUpvalues;

    /* Iterate past upvalues pointing to slots above the given one */
    while (upvalue != NULL && upvalue->slot > local) {
        prevUpvalue = upvalue;
        upvalue = upvalue->next;
    }

    if (upvalue != NULL && upvalue->slot == local) /* Checks if already exists in the list */
        return upvalue;

    ObjUpvalue *createdUpvalue = newUpvalue(vm, local); /* Creates a new upvalue */
    createdUpvalue->next = upvalue;                     /* Adds to the list */

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
static void closeUpvalues(VM *vm, Value *last) {
    while (vm->openUpvalues != NULL && vm->openUpvalues->slot >= last) {
        ObjUpvalue *upvalue = vm->openUpvalues;
        upvalue->closed = *upvalue->slot;
        upvalue->slot = &upvalue->closed;
        vm->openUpvalues = upvalue->next;
    }
}

/**
 * Concatenates the two values on the top of the virtual machine stack. Then, pushes the new string
 * to the stack.
 */
static void concatenateStrings(VM *vm) {
    ObjString *b = AS_STRING(pop(vm));
    ObjString *a = AS_STRING(vm->stackTop[-1]);
    ObjString *result = concatStrings(vm, b, a); /* Concatenate both strings */
    vm->stackTop[-1] = OBJ_VAL(result);          /* Update the stack top */
    tableSet(&vm->strings, result, NULL_VAL);    /* Intern the string */
}

/**
 * Loops through all the instructions in a bytecode chunk. Each turn through the loop, it reads and
 * executes the current bytecode instruction.
 */
static ResultCode run(VM *vm) {
    CallFrame *frame = &vm->frames[vm->frameCount - 1]; /* Current call frame */

/* Reads the next 8 bits (byte) or 16 bits (2 bytes) */
#define READ_BYTE()  (*frame->pc++)
#define READ_SHORT() (frame->pc += 2, (uint16_t) ((frame->pc[-2] << 8) | frame->pc[-1]))

/* Reads the next byte from the bytecode, treats the resulting number as an index, and looks up the
 * corresponding location in the chunk’s constant table */
#define READ_CONSTANT() (frame->closure->function->bytecodeChunk.constants.values[READ_BYTE()])
#define READ_STRING()   AS_STRING(READ_CONSTANT())

/* Checks if the two elements at the top of the YAPL VM's stack are numerical Values. If not, a
 * runtime error is returned */
#define ASSERT_TOP2_NUM(vm)                                 \
    do {                                                    \
        if (!IS_NUM(peek(vm, 0)) || !IS_NUM(peek(vm, 1))) { \
            VMError(vm, OPR_NOT_NUM_ERR);                   \
            return RUNTIME_ERROR;                           \
        }                                                   \
    } while (false)

/* Checks if the element at the top of the YAPL VM's stack is a numerical Value. If not, a runtime
 * error is returned */
#define ASSERT_TOP_NUM(vm)                \
    do {                                  \
        if (!IS_NUM(peek(vm, 0))) {       \
            VMError(vm, OPR_NOT_NUM_ERR); \
            return RUNTIME_ERROR;         \
        }                                 \
    } while (false)

/* Checks if the element at the top of the YAPL VM's stack is not zero. If not, a runtime error is
 * returned */
#define ASSERT_TOP_NOT_0(vm)            \
    do {                                \
        if (AS_NUM(peek(vm, 0)) == 0) { \
            VMError(vm, DIV_ZERO_ERR);  \
            return RUNTIME_ERROR;       \
        }                               \
    } while (false)

/* Performs a binary operation of the 'op' operator on the two elements on the top of the YAPL VM's
 * stack. Then, sets the result on the top of the stack */
#define BINARY_OP(vm, op, valueType)          \
    do {                                      \
        ASSERT_TOP2_NUM(vm);                  \
        double b = AS_NUM(pop(vm));           \
        double a = AS_NUM(vm->stackTop[-1]);  \
        vm->stackTop[-1] = valueType(a op b); \
    } while (false)

/* Performs a division operation (integer mod or double division) on the two elements on the top of
 * the YAPL VM's stack. Then, sets the result on the top of the stack */
#define DIVISION_OP(vm, op, type)           \
    do {                                    \
        ASSERT_TOP2_NUM(vm);                \
        ASSERT_TOP_NOT_0(vm);               \
        type b = AS_NUM(pop(vm));           \
        type a = AS_NUM(vm->stackTop[-1]);  \
        vm->stackTop[-1] = NUM_VAL(a op b); \
    } while (false)

#ifdef YAPL_DEBUG_TRACE_EXECUTION
    printTraceExecutionHeader();
#endif

    while (true) {
#ifdef YAPL_DEBUG_TRACE_EXECUTION
        if (vm->stack != vm->stackTop) printStack(vm);
        disassembleInstruction(&frame->closure->function->bytecodeChunk,
                               (int) (frame->pc - frame->closure->function->bytecodeChunk.code));
#endif

        uint8_t instruction = READ_BYTE();
        switch (instruction) { /* Reads the next byte and switches through the opcodes */

            /* Constants and literals */
            case OP_CONSTANT: {
                uint16_t index = READ_BYTE() | READ_BYTE() << 8;
                if (!push(vm, frame->closure->function->bytecodeChunk.constants.values[index]))
                    return RUNTIME_ERROR;
                break;
            }
            case OP_FALSE:
                if (!push(vm, BOOL_VAL(false))) return RUNTIME_ERROR;
                break;
            case OP_TRUE:
                if (!push(vm, BOOL_VAL(true))) return RUNTIME_ERROR;
                break;
            case OP_NULL:
                if (!push(vm, NULL_VAL)) return RUNTIME_ERROR;
                break;

            /* Relational operations */
            case OP_AND: {
                uint16_t offset = READ_SHORT();
                if (isFalsey(peek(vm, 0)))
                    frame->pc += offset;
                else
                    pop(vm);
                break;
            }
            case OP_OR: {
                uint16_t offset = READ_SHORT();
                if (isFalsey(peek(vm, 0)))
                    pop(vm);
                else
                    frame->pc += offset;
                break;
            }
            case OP_NOT:
                vm->stackTop[-1] = BOOL_VAL(isFalsey(vm->stackTop[-1]));
                break;
            case OP_EQUAL: {
                Value b = pop(vm);
                vm->stackTop[-1] = BOOL_VAL(valuesEqual(vm->stackTop[-1], b));
                break;
            }
            case OP_GREATER:
                BINARY_OP(vm, >, BOOL_VAL);
                break;
            case OP_LESS:
                BINARY_OP(vm, <, BOOL_VAL);
                break;

            /* Arithmetic operations */
            case OP_ADD: {
                if (IS_STRING(peek(vm, 0)) && IS_STRING(peek(vm, 1))) {
                    concatenateStrings(vm);
                } else if (IS_NUM(peek(vm, 0)) && IS_NUM(peek(vm, 1))) {
                    double a = AS_NUM(pop(vm));
                    vm->stackTop[-1] = NUM_VAL(AS_NUM(vm->stackTop[-1]) + a);
                } else {
                    VMError(vm, OPR_NOT_NUM_STR_ERR);
                    return RUNTIME_ERROR;
                }
                break;
            }
            case OP_SUBTRACT:
                BINARY_OP(vm, -, NUM_VAL);
                break;
            case OP_NEGATE:
                ASSERT_TOP_NUM(vm);
                vm->stackTop[-1] = NUM_VAL(-AS_NUM(vm->stackTop[-1]));
                break;
            case OP_MULTIPLY:
                BINARY_OP(vm, *, NUM_VAL);
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
                double a = AS_NUM(pop(vm));
                vm->stackTop[-1] = NUM_VAL(YAPL_POW(AS_NUM(vm->stackTop[-1]), a));
                break;
            }

            /* Variable operations */
            case OP_DEFINE_GLOBAL: {
                ObjString *name = READ_STRING();
                tableSet(&vm->globals, name, peek(vm, 0));
                pop(vm);
                break;
            }
            case OP_GET_GLOBAL: {
                ObjString *name = READ_STRING();
                Value value;
                if (!tableGet(&vm->globals, name, &value)) /* Checks if variable is undefined */
                    return undefinedVariableError(vm, name, false);
                if (!push(vm, value)) return RUNTIME_ERROR;
                break;
            }
            case OP_SET_GLOBAL: {
                ObjString *name = READ_STRING();
                if (tableSet(&vm->globals, name, peek(vm, 0))) /* Checks if variable is undefined */
                    return undefinedVariableError(vm, name, true);
                break;
            }
            case OP_GET_UPVALUE: {
                uint8_t slot = READ_BYTE();
                if (!push(vm, *frame->closure->upvalues[slot]->slot)) return RUNTIME_ERROR;
                break;
            }
            case OP_SET_UPVALUE: {
                uint8_t slot = READ_BYTE();
                *frame->closure->upvalues[slot]->slot = peek(vm, 0);
                break;
            }
            case OP_CLOSE_UPVALUE:
                closeUpvalues(vm, vm->stackTop - 1);
                pop(vm);
                break;
            case OP_GET_LOCAL: {
                uint8_t slot = READ_BYTE();
                if (!push(vm, frame->slots[slot])) return RUNTIME_ERROR;
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
                if (isFalsey(peek(vm, 0))) frame->pc += offset;
                break;
            }
            case OP_LOOP: {
                uint16_t offset = READ_SHORT();
                frame->pc -= offset;
                break;
            }

            /* Function operations */
            case OP_CLOSURE: {
                ObjFunction *function = AS_FUNCTION(READ_CONSTANT());
                ObjClosure *closure = newClosure(vm, function);
                if (!push(vm, OBJ_VAL(closure))) return RUNTIME_ERROR;

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
                if (!callValue(vm, peek(vm, argCount), argCount)) return RUNTIME_ERROR;
                frame = &vm->frames[vm->frameCount - 1]; /* Updates the current frame */
                break;
            }
            case OP_RETURN: {
                Value result = pop(vm);          /* Gets the function's return value */
                closeUpvalues(vm, frame->slots); /* Closes upvalues */
                vm->frameCount--;

                if (vm->frameCount == 0) { /* Checks if top level code is finished */
                    pop(vm);               /* Pops "script" from the stack */
                    return OK;
                }

                vm->stackTop = frame->slots;                 /* Resets the stack top */
                if (!push(vm, result)) return RUNTIME_ERROR; /* Pushes the return value */
                frame = &vm->frames[vm->frameCount - 1];     /* Updates the current frame */
                break;
            }

            /* VM operations */
            case OP_DUP:
                push(vm, peek(vm, 0));
                break;
            case OP_POP:
                pop(vm);
                break;
            case OP_POP_EXPR: {
                Value result = pop(vm);
                bool isString = IS_STRING(result);
                printf(" => ");
                if (isString) printf("\"");
                printValue(result);
                if (isString) printf("\"");
                printf("\n");
                break;
            }
            case OP_TEMP:
                VMError(vm, UNREACHABLE_ERR, instruction);
                return RUNTIME_ERROR;

            /* Unknown opcode */
            default:
                VMError(vm, UNKNOWN_OPCODE_ERR, instruction);
                return RUNTIME_ERROR;
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
}

/**
 * Interprets a YAPL's source code string. If the source is compiled successfully, the bytecode
 * chunk is set for the YAPL's virtual machine to execute.
 */
ResultCode interpret(VM *vm, const char *source) {
    ObjFunction *function = compile(vm, source); /* Compiles the source code */
    if (function == NULL) return COMPILE_ERROR;

    /* Set the script to run */
    push(vm, OBJ_VAL(function));
    ObjClosure* closure = newClosure(vm, function);
    pop(vm);
    push(vm, OBJ_VAL(closure));
    callValue(vm, OBJ_VAL(closure), 0);

    return run(vm); /* Executes the bytecode chunk */
}
