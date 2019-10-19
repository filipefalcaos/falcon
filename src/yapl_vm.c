/*
 * YAPL version 0.0.1 (master, Oct 18 2019)
 * yapl_vm.c: YAPL's stack-based virtual machine
 * See YAPL's license in the LICENSE file
 */

#include "../include/yapl_vm.h"
#include "../include/yapl_debug.h"
#include <stdio.h>

/* VM instance */
VM vm;

/**
 * Resets the virtual machine stack.
 */
static void resetVMStack() {
    vm.stackTop = vm.stack;
}

/**
 * Initializes the YAPL's virtual machine.
 */
void initVM() {
    resetVMStack();
}

/**
 * Frees the YAPL's virtual machine and its allocated objects.
 */
void freeVM() {}

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
 * Prints the YAPL's virtual machine stack.
 */
void printStack() {
    printf("          ");
    for (Value* slot = vm.stack; slot < vm.stackTop; slot++) {
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

/* Performs a binary operation of the 'op' operator on the two elements on the top of the YAPL VM's
 * stack. Then, returns the result */
#define BINARY_OP(op) \
    do { \
        double b = pop(); \
        double a = pop(); \
        push(a op b); \
    } while (false)

#ifdef __DEBUG_TRACE_EXECUTION__
    printTraceExecutionHeader();
#endif

    while (true) {
#ifdef __DEBUG_TRACE_EXECUTION__

        printStack();
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

            /* Arithmetic operations */
            case OP_ADD:
                BINARY_OP(+);
                break;
            case OP_SUBTRACT:
                BINARY_OP(-);
                break;
            case OP_NEGATE:
                push(-pop());
                break;
            case OP_MULTIPLY:
                BINARY_OP(*);
                break;
            case OP_DIVIDE:
                BINARY_OP(/);
                break;

            /* Function operations */
            case OP_RETURN:
                printValue(pop());
                printf("\n");
                return OK;
        }
    }

#undef READ_BYTE
#undef READ_CONSTANT
#undef BINARY_OP
}

/**
 * Interprets a YAPL's source code string. If the source is compiled successfully, the bytecode
 * chunk is set for the YAPL's virtual machine to execute.
 */
ResultCode interpret(BytecodeChunk *bytecodeChunk) {
    vm.bytecodeChunk = bytecodeChunk;
    vm.pc = vm.bytecodeChunk->code;
    return run();
}
