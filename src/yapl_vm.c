/*
 * YAPL version 0.0.1 (master, Oct 18 2019)
 * yapl_vm.c: YAPL's stack-based virtual machine
 * See YAPL's license in the LICENSE file
 */

#include "../include/yapl_vm.h"
#include "../include/yapl_compiler.h"
#include "../include/yapl_memory_manager.h"
#include "../include/yapl_object.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#ifdef YAPL_DEBUG_TRACE_EXECUTION
#include "../include/yapl_debug.h"
#endif

/* VM instance */
VM vm;

/**
 * Resets the virtual machine stack.
 */
static void resetVMStack() { vm.stackTop = vm.stack; }

/**
 * Presents a runtime error to the programmer.
 */
void runtimeError(const char *format, ...) {
    va_list args;
    va_start(args, format); /* Gets the arguments */
    va_end(args);

    size_t instruction = vm.pc - vm.bytecodeChunk->code;     /* Gets the instruction */
    int line = getSourceLine(vm.bytecodeChunk, instruction); /* Gets the line */

    fprintf(stderr, "File \"<stdin>\", line %d, in <script>\n", line);
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    resetVMStack(); /* Resets the stack due to error */
}

/**
 * Initializes the YAPL's virtual machine.
 */
void initVM() {
    resetVMStack();
    initTable(&vm.strings);
    initTable(&vm.globals);
    vm.objects = NULL;
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
 * Loops through all the instructions in a bytecode chunk. Each turn through the loop, it reads and
 * executes the current bytecode instruction.
 */
static ResultCode run() {

/* Reads the current byte */
#define READ_BYTE() (*vm.pc++)

/* Reads the next byte from the bytecode, treats the resulting number as an index, and looks up the
 * corresponding location in the chunk’s constant table */
#define READ_CONSTANT() (vm.bytecodeChunk->constants.values[READ_BYTE()])
#define READ_STRING()   AS_STRING(READ_CONSTANT())

/* Performs a binary operation of the 'op' operator on the two elements on the top of the YAPL VM's
 * stack. Then, returns the result */
#define BINARY_OP(op, valueType, type)                    \
    do {                                                  \
        if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) { \
            runtimeError("Operands must be numbers.");    \
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
        disassembleInstruction(vm.bytecodeChunk, (int) (vm.pc - vm.bytecodeChunk->code));
#endif

        switch (READ_BYTE()) { /* Reads the next byte and switches through the opcodes */

            /* Constants and literals */
            case OP_CONSTANT:
                push(READ_CONSTANT());
                break;
            case OP_CONSTANT_16: {
                uint16_t index = READ_BYTE() | READ_BYTE() << 8;
                push(vm.bytecodeChunk->constants.values[index]);
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
                    runtimeError("Operands must be two numbers or two strings.");
                    return RUNTIME_ERROR;
                }
                break;
            }
            case OP_SUBTRACT:
                BINARY_OP(-, NUMBER_VAL, double);
                break;
            case OP_NEGATE:
                if (!IS_NUMBER(peek(0))) {
                    runtimeError("Operand must be a number.");
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
            case OP_DEFINE_GLOBAL:
                tableSet(&vm.globals, READ_STRING(), peek(0));
                pop();
                break;
            case OP_GET_GLOBAL: {
                ObjString *name = READ_STRING();
                Value value;

                if (!tableGet(&vm.globals, name, &value)) {
                    runtimeError("Undefined variable '%s'.", name->chars);
                    return RUNTIME_ERROR;
                }

                push(value);
                break;
            }
            case OP_SET_GLOBAL: {
                ObjString *name = READ_STRING();
                if (tableSet(&vm.globals, name, peek(0))) {
                    tableDelete(&vm.globals, name);
                    runtimeError("Undefined variable '%s'.", name->chars);
                    return RUNTIME_ERROR;
                }
                break;
            }

            /* Print operations */
            case OP_PRINT:
                printValue(pop());
                printf("\n");
                break;

            /* Function operations */
            case OP_RETURN:
                return OK;

            /* VM operations */
            case OP_POP:
                pop();
                break;
        }
    }

#undef READ_BYTE
#undef READ_CONSTANT
#undef READ_STRING
#undef BINARY_OP
}

/**
 * Interprets a YAPL's source code string. If the source is compiled successfully, the bytecode
 * chunk is set for the YAPL's virtual machine to execute.
 */
ResultCode interpret(const char *source) {
    BytecodeChunk bytecodeChunk;
    initBytecodeChunk(&bytecodeChunk);

    if (!compile(source, &bytecodeChunk)) { /* Compiles the source code */
        freeBytecodeChunk(&bytecodeChunk);
        return COMPILE_ERROR;
    }

    vm.bytecodeChunk = &bytecodeChunk;
    vm.pc = vm.bytecodeChunk->code;
    ResultCode resultCode = run(); /* Executes the bytecode chunk */
    freeBytecodeChunk(&bytecodeChunk);
    return resultCode;
}
