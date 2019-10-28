/*
 * YAPL version 0.0.1 (master, Oct 18 2019)
 * yapl_vm.c: YAPL's stack-based virtual machine
 * See YAPL's license in the LICENSE file
 */

#include "yapl_vm.h"
#include "../compiler/yapl_compiler.h"
#include "../lib/yapl_natives.h"
#include "yapl_memory_manager.h"
#include "yapl_object.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#ifdef YAPL_DEBUG_TRACE_EXECUTION
#include "../lib/yapl_debug.h"
#endif

/* VM instance */
VM vm;

/**
 * Resets the virtual machine stack.
 */
static void resetVMStack() {
    vm.stackTop = vm.stack;
    vm.openUpvalues = NULL;
    vm.frameCount = 0;
}

/**
 * Presents a runtime error to the programmer.
 */
void runtimeError(const char *format, ...) {
    va_list args;
    va_start(args, format); /* Gets the arguments */
    va_end(args);

    /* Prints the error */
    fprintf(stderr, "RuntimeError: ");
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");

    /* Prints a stack trace */
    fprintf(stderr, "Stack trace (last call first):\n");
    for (int i = vm.frameCount - 1; i >= 0; i--) {
        CallFrame *currentFrame = &vm.frames[i];
        ObjFunction *currentFunction = currentFrame->closure->function;
        size_t currentInstruction = currentFrame->pc - currentFunction->bytecodeChunk.code - 1;
        int currentLine = getSourceLine(&currentFrame->closure->function->bytecodeChunk,
                                        (int) currentInstruction);

        fprintf(stderr, "    [Line %d] in ", currentLine);
        if (currentFunction->name == NULL) {
            fprintf(stderr, "%s\n", SCRIPT_TAG);
        } else {
            fprintf(stderr, "%s()\n", currentFunction->name->chars);
        }
    }

    resetVMStack(); /* Resets the stack due to error */
}

/**
 * Presents a runtime error when a global variable is undefined.
 */
static ResultCode undefinedVariableError(ObjString *name, bool delete) {
    if (delete) tableDelete(&vm.globals, name);
    runtimeError(UNDEF_VAR_ERR, name->chars);
    return RUNTIME_ERROR;
}

/**
 * Presents a runtime error when a global variable is already declared.
 */
static ResultCode declaredVariableError(ObjString *name) {
    runtimeError(GLB_VAR_REDECL_ERR, name->chars);
    return RUNTIME_ERROR;
}

/**
 * Defines a new native function for YAPL.
 */
static void defineNative(const char *name, NativeFn function) {
    push(OBJ_VAL(copyString(name, (int) strlen(name))));
    push(OBJ_VAL(newNative(function)));
    tableSet(&vm.globals, AS_STRING(vm.stack[0]), vm.stack[1]);
    pop();
    pop();
}

/**
 * Initializes the YAPL's virtual machine.
 */
void initVM(const char *fileName) {
    resetVMStack();
    initTable(&vm.strings);
    initTable(&vm.globals);
    vm.fileName = fileName;
    vm.objects = NULL;

    /* Set native functions */
    defineNative("clock", clockNative);
    defineNative("print", printNative);
    defineNative("puts", putsNative);
}

/**
 * Frees the YAPL's virtual machine and its allocated objects.
 */
void freeVM() {
    freeTable(&vm.strings);
    freeTable(&vm.globals);
    freeObjects();
}

/**
 * Pushes a value to the YAPL's virtual machine stack.
 */
void push(Value value) {
    *vm.stackTop = value;
    vm.stackTop++;
}

/**
 * Pops a value from the YAPL's virtual machine stack.
 */
Value pop() {
    vm.stackTop--;
    return *vm.stackTop;
}

/**
 * Peeks a element on the YAPL's virtual machine stack.
 */
static Value peek(int distance) { return vm.stackTop[-1 - distance]; }

/**
 * Prints the YAPL's virtual machine stack.
 */
void printStack() {
    printf("          ");
    for (Value *slot = vm.stack; slot < vm.stackTop; slot++) {
        printf("[ ");
        printValue(*slot);
        printf(" ]");
    }
    printf("\n");
}

/**
 * Executes a call on the given YAPL function by setting its call frame to be run.
 */
static bool call(ObjClosure *closure, int argCount) {
    if (argCount != closure->function->arity) {
        runtimeError(ARGS_COUNT_ERR, closure->function->arity, argCount);
        return false;
    }

    if (vm.frameCount == VM_FRAMES_MAX) {
        runtimeError(STACK_OVERFLOW);
        return false;
    }

    CallFrame *frame = &vm.frames[vm.frameCount++];
    frame->closure = closure;
    frame->pc = closure->function->bytecodeChunk.code;
    frame->slots = vm.stackTop - argCount - 1;
    return true;
}

/**
 * Tries to execute a function call on a given YAPL value.
 */
static bool callValue(Value callee, int argCount) {
    if (IS_OBJ(callee)) {
        switch (OBJ_TYPE(callee)) {
            case OBJ_CLOSURE:
                return call(AS_CLOSURE(callee), argCount);
            case OBJ_NATIVE: {
                NativeFn native = AS_NATIVE(callee);
                Value result = native(argCount, vm.stackTop - argCount);
                vm.stackTop -= argCount + 1;
                push(result);
                return true;
            }
            default:
                break; /* Not callable */
        }
    }

    runtimeError(VALUE_NOT_CALL_ERR);
    return false;
}

/**
 * Captures a given local upvalue.
 */
static ObjUpvalue *captureUpvalue(Value *local) {
    ObjUpvalue *prevUpvalue = NULL;
    ObjUpvalue *upvalue = vm.openUpvalues;

    /* Iterate past upvalues pointing to slots above the given one */
    while (upvalue != NULL && upvalue->slot > local) {
        prevUpvalue = upvalue;
        upvalue = upvalue->next;
    }

    if (upvalue != NULL && upvalue->slot == local) /* Checks if already exists in the list */
        return upvalue;

    ObjUpvalue *createdUpvalue = newUpvalue(local); /* Creates a new upvalue */
    createdUpvalue->next = upvalue;                 /* Adds to the list */

    if (prevUpvalue == NULL) {
        vm.openUpvalues = createdUpvalue;
    } else {
        prevUpvalue->next = createdUpvalue;
    }

    return createdUpvalue;
}

/**
 * Closes the upvalues for a given stack slot.
 */
static void closeUpvalues(Value *last) {
    while (vm.openUpvalues != NULL && vm.openUpvalues->slot >= last) {
        ObjUpvalue *upvalue = vm.openUpvalues;
        upvalue->closed = *upvalue->slot;
        upvalue->slot = &upvalue->closed;
        vm.openUpvalues = upvalue->next;
    }
}

/**
 * Takes the logical not (falsiness) of a value (boolean or null). In YAPL, 'null', 'false', the
 * number zero, and an empty string are falsey, while every other value behaves like 'true'.
 */
static bool isFalsey(Value value) {
    return IS_NULL(value) || (IS_BOOL(value) && !AS_BOOL(value)) ||
           (IS_NUMBER(value) && AS_NUMBER(value) == 0);
}

/**
 * Concatenates the two values on the top of the virtual machine stack. Then, pushes the new string
 * to the stack.
 */
static void concatenateStrings() {
    ObjString *b = AS_STRING(pop());
    ObjString *a = AS_STRING(vm.stackTop[-1]);

    /* Concatenate both strings */
    int length = a->length + b->length;
    ObjString *result = makeString(length);
    memcpy(result->chars, a->chars, a->length);
    memcpy(result->chars + a->length, b->chars, b->length);
    result->chars[length] = '\0';
    result->hash = hashString(result->chars, length);

    vm.stackTop[-1] = OBJ_VAL(result);       /* Update the stack top */
    tableSet(&vm.strings, result, NULL_VAL); /* Intern the string */
}

/**
 * Loops through all the instructions in a bytecode chunk. Each turn through the loop, it reads and
 * executes the current bytecode instruction.
 */
static ResultCode run() {
    CallFrame *frame = &vm.frames[vm.frameCount - 1]; /* Current call frame */

/* Reads the current 8 bits (byte) or 16 bits (2 bytes) */
#define READ_BYTE()  (*frame->pc++)
#define READ_SHORT() (frame->pc += 2, (uint16_t) ((frame->pc[-2] << 8) | frame->pc[-1]))

/* Reads the next byte from the bytecode, treats the resulting number as an index, and looks up the
 * corresponding location in the chunk’s constant table */
#define READ_CONSTANT() (frame->closure->function->bytecodeChunk.constants.values[READ_BYTE()])
#define READ_STRING()   AS_STRING(READ_CONSTANT())

/* Performs a binary operation of the 'op' operator on the two elements on the top of the YAPL VM's
 * stack. Then, returns the result */
#define BINARY_OP(op, valueType, type)                    \
    do {                                                  \
        if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) { \
            runtimeError(OPR_NOT_NUM_ERR);    \
            return RUNTIME_ERROR;                         \
        }                                                 \
        type b = AS_NUMBER(pop());                        \
        type a = AS_NUMBER(vm.stackTop[-1]);              \
        vm.stackTop[-1] = valueType((type) a op b);       \
    } while (false)

#ifdef YAPL_DEBUG_TRACE_EXECUTION
    printTraceExecutionHeader();
#endif

    while (true) {
#ifdef YAPL_DEBUG_TRACE_EXECUTION
        if (vm.stack != vm.stackTop) printStack();
        disassembleInstruction(&frame->closure->function->bytecodeChunk,
                               (int) (frame->pc - frame->closure->function->bytecodeChunk.code));
#endif

        uint8_t instruction = READ_BYTE();
        switch (instruction) { /* Reads the next byte and switches through the opcodes */

            /* Constants and literals */
            case OP_CONSTANT:
                push(READ_CONSTANT());
                break;
            case OP_CONSTANT_16: {
                uint16_t index = READ_BYTE() | READ_BYTE() << 8;
                push(frame->closure->function->bytecodeChunk.constants.values[index]);
                break;
            }
            case OP_FALSE:
                push(BOOL_VAL(false));
                break;
            case OP_TRUE:
                push(BOOL_VAL(true));
                break;
            case OP_NULL:
                push(NULL_VAL);
                break;

            /* Relational operations */
            case OP_AND: {
                uint16_t offset = READ_SHORT();
                if (isFalsey(peek(0)))
                    frame->pc += offset;
                else
                    pop();
                break;
            }
            case OP_OR: {
                uint16_t offset = READ_SHORT();
                if (isFalsey(peek(0)))
                    pop();
                else
                    frame->pc += offset;
                break;
            }
            case OP_NOT:
                vm.stackTop[-1] = BOOL_VAL(isFalsey(vm.stackTop[-1]));
                break;
            case OP_EQUAL: {
                Value b = pop();
                vm.stackTop[-1] = BOOL_VAL(valuesEqual(vm.stackTop[-1], b));
                break;
            }
            case OP_GREATER:
                BINARY_OP(>, BOOL_VAL, double);
                break;
            case OP_LESS:
                BINARY_OP(<, BOOL_VAL, double);
                break;

            /* Arithmetic operations */
            case OP_ADD: {
                if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
                    concatenateStrings();
                } else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
                    double a = AS_NUMBER(pop());
                    vm.stackTop[-1] = NUMBER_VAL(AS_NUMBER(vm.stackTop[-1]) + a);
                } else {
                    runtimeError(OPR_NOT_NUM_STR_ERR);
                    return RUNTIME_ERROR;
                }
                break;
            }
            case OP_SUBTRACT:
                BINARY_OP(-, NUMBER_VAL, double);
                break;
            case OP_NEGATE:
                if (!IS_NUMBER(peek(0))) {
                    runtimeError(OPR_NOT_NUM_ERR);
                    return RUNTIME_ERROR;
                }
                vm.stackTop[-1] = NUMBER_VAL(-AS_NUMBER(vm.stackTop[-1]));
                break;
            case OP_MULTIPLY:
                BINARY_OP(*, NUMBER_VAL, double);
                break;
            case OP_MOD:
                BINARY_OP(%, NUMBER_VAL, int);
                break;
            case OP_DIVIDE:
                BINARY_OP(/, NUMBER_VAL, double);
                break;

            /* Variable operations */
            case OP_DEFINE_GLOBAL: {
                ObjString *name = READ_STRING();
                Value value;
                if (tableGet(&vm.globals, name, &value)) /* Checks if already declared */
                    return declaredVariableError(name);
                tableSet(&vm.globals, name, peek(0));
                pop();
                break;
            }
            case OP_GET_GLOBAL: {
                ObjString *name = READ_STRING();
                Value value;
                if (!tableGet(&vm.globals, name, &value)) /* Checks if variable is undefined */
                    return undefinedVariableError(name, false);
                push(value);
                break;
            }
            case OP_SET_GLOBAL: {
                ObjString *name = READ_STRING();
                if (tableSet(&vm.globals, name, peek(0))) /* Checks if variable is undefined */
                    return undefinedVariableError(name, true);
                break;
            }
            case OP_GET_UPVALUE: {
                uint8_t slot = READ_BYTE();
                push(*frame->closure->upvalues[slot]->slot);
                break;
            }
            case OP_SET_UPVALUE: {
                uint8_t slot = READ_BYTE();
                *frame->closure->upvalues[slot]->slot = peek(0);
                break;
            }
            case OP_CLOSE_UPVALUE:
                closeUpvalues(vm.stackTop - 1);
                pop();
                break;
            case OP_GET_LOCAL: {
                uint8_t slot = READ_BYTE();
                push(frame->slots[slot]);
                break;
            }
            case OP_SET_LOCAL: {
                uint8_t slot = READ_BYTE();
                frame->slots[slot] = peek(0);
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
                if (isFalsey(peek(0))) frame->pc += offset;
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
                ObjClosure *closure = newClosure(function);
                push(OBJ_VAL(closure));

                /* Capture upvalues */
                for (int i = 0; i < closure->upvalueCount; i++) {
                    uint8_t isLocal = READ_BYTE();
                    uint8_t index = READ_BYTE();

                    if (isLocal) {
                        closure->upvalues[i] = captureUpvalue(frame->slots + index);
                    } else {
                        closure->upvalues[i] = frame->closure->upvalues[index];
                    }
                }

                break;
            }
            case OP_CALL: {
                int argCount = READ_BYTE();
                if (!callValue(peek(argCount), argCount)) return RUNTIME_ERROR;
                frame = &vm.frames[vm.frameCount - 1]; /* Updates the current frame */
                break;
            }
            case OP_RETURN: {
                Value result = pop();        /* Gets the function's return value */
                closeUpvalues(frame->slots); /* Closes upvalues */
                vm.frameCount--;

                if (vm.frameCount == 0) { /* Checks if top level code is finished */
                    pop();                /* Pops "script" from the stack */
                    return OK;
                }

                vm.stackTop = frame->slots;            /* Resets the stack top */
                push(result);                          /* Pushes the function's return value */
                frame = &vm.frames[vm.frameCount - 1]; /* Updates the current frame */
                break;
            }

            /* VM operations */
            case OP_POP:
                pop();
                break;

            /* Unknown opcode */
            default:
                runtimeError(UNKNOWN_OPCODE_ERR, instruction);
                break;
        }
    }

#undef READ_BYTE
#undef READ_SHORT
#undef READ_CONSTANT
#undef READ_STRING
#undef BINARY_OP
}

/**
 * Interprets a YAPL's source code string. If the source is compiled successfully, the bytecode
 * chunk is set for the YAPL's virtual machine to execute.
 */
ResultCode interpret(const char *source) {
    ObjFunction *function = compile(source); /* Compiles the source code */
    if (function == NULL) return COMPILE_ERROR;

    /* Set the script to run */
    push(OBJ_VAL(function));
    ObjClosure* closure = newClosure(function);
    pop();
    push(OBJ_VAL(closure));
    callValue(OBJ_VAL(closure), 0);

    return run(); /* Executes the bytecode chunk */
}
