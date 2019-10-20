/*
 * YAPL version 0.0.1 (master, Oct 18 2019)
 * yapl_debug.c: List of debugging function for the YAPL virtual machine
 * See YAPL's license in the LICENSE file
 */

#include "../include/yapl_debug.h"
#include "../include/yapl_value.h"
#include <stdio.h>

/**
 * Print debugging (trace execution) header.
 */
void printTraceExecutionHeader() {
    printf("=============================================================\n");
    printf("================ DEBUGGING - TRACE EXECUTION ================\n");
    printf("=============================================================\n");
}

/**
 * Displays a simple bytecode instruction.
 */
static int simpleInstruction(const char *name, int offset) {
    printf("%s\n", name);
    return offset + 1;
}

/**
 * Displays a constant instruction (8 bits).
 */
static int constantInstruction(const char *name, BytecodeChunk *bytecodeChunk, int offset) {
    uint8_t constant = bytecodeChunk->code[offset + 1];
    printf("%-16s %4d '", name, constant);
    printValue(bytecodeChunk->constants.values[constant]);
    printf("'\n");
    return offset + 2;
}

/**
 * Disassembles a single instruction in a bytecode chunk.
 */
int disassembleInstruction(BytecodeChunk *bytecodeChunk, int offset) {
    printf("%04d ", offset); /* Prints offset info */
    int sourceLine = getSourceLine(bytecodeChunk, offset);

    /* Prints line info */
    if (offset > 0 && sourceLine == getSourceLine(bytecodeChunk, offset - 1)) {
        printf("   | ");
    } else {
        printf("%4d ", sourceLine);
    }

    uint8_t instruction = bytecodeChunk->code[offset]; /* Current instruction */
    switch (instruction) { /* Verifies the instruction type */
        case OP_CONSTANT:
            return constantInstruction("OP_CONSTANT", bytecodeChunk, offset);
        case OP_ADD:
            return simpleInstruction("OP_ADD", offset);
        case OP_SUBTRACT:
            return simpleInstruction("OP_SUBTRACT", offset);
        case OP_NEGATE:
            return simpleInstruction("OP_NEGATE", offset);
        case OP_MULTIPLY:
            return simpleInstruction("OP_MULTIPLY", offset);
        case OP_MOD:
            return simpleInstruction("OP_MOD", offset);
        case OP_DIVIDE:
            return simpleInstruction("OP_DIVIDE", offset);
        case OP_RETURN:
            return simpleInstruction("OP_RETURN", offset);
        default:
            printf("Unknown opcode %d\n", instruction);
            return offset + 1;
    }
}

/**
 * Disassembles a complete bytecode chunk.
 */
void disassembleBytecodeChunk(BytecodeChunk *bytecodeChunk, const char *name) {
    printf("== %s ==\n", name);

    for (int offset = 0; offset < bytecodeChunk->count;) {      /* Loop through the instructions */
        offset = disassembleInstruction(bytecodeChunk, offset); /* Disassemble instruction */
    }
}
