/*
 * YAPL version 0.0.1 (master, Oct 18 2019)
 * yapl_debug.c: List of debugging function for the YAPL virtual machine
 * See YAPL's license in the LICENSE file
 */

#include "../include/yapl_debug.h"
#include "../include/yapl_value.h"
#include <stdio.h>

/**
 * Print debugging (opcodes) header.
 */
void printOpcodesHeader() {
    printf("=============================================================\n");
    printf("================= DEBUGGING - PRINT OPCODES =================\n");
    printf("=============================================================\n");
}

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
 * Displays a local variable instruction.
 */
static int byteInstruction(const char *name, BytecodeChunk *bytecodeChunk, int offset) {
    uint8_t slot = bytecodeChunk->code[offset + 1];
    printf("%-16s %4d\n", name, slot);
    return offset + 2;
}

/**
 * Displays a jump (conditional) instruction.
 */
static int jumpInstruction(const char *name, int sign, BytecodeChunk *bytecodeChunk, int offset) {
    uint16_t jump = (uint16_t) (bytecodeChunk->code[offset + 1] << 8);
    jump |= bytecodeChunk->code[offset + 2];
    printf("%-16s %4d -> %d\n", name, offset, offset + 3 + sign * jump);
    return offset + 3;
}

/**
 * Displays a constant instruction (8 bits).
 */
static int constantInstruction(const char *name, BytecodeChunk *bytecodeChunk, int offset) {
    uint8_t constant = bytecodeChunk->code[offset + 1];
    Value value = bytecodeChunk->constants.values[constant];

    /* Prints the constant */
    printf("%-16s %4d ", name, constant);
    if (value.type != VAL_OBJ) printf("'");
    printValue(value, false);
    if (value.type != VAL_OBJ) printf("'");
    printf("\n");
    return offset + 2;
}

/**
 * Displays a constant instruction (16 bits).
 */
static int constantInstruction16(const char *name, BytecodeChunk *bytecodeChunk, int offset) {
    uint16_t constant = bytecodeChunk->code[offset + 1] | (bytecodeChunk->code[offset + 2] << 8);
    Value value = bytecodeChunk->constants.values[constant];

    /* Prints the constant */
    printf("%-16s %4d ", name, constant);
    if (value.type != VAL_OBJ) printf("'");
    printValue(value, false);
    if (value.type != VAL_OBJ) printf("'");
    printf("\n");
    return offset + 3;
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
        case OP_CONSTANT_16:
            return constantInstruction16("OP_CONSTANT_16", bytecodeChunk, offset);
        case OP_FALSE:
            return simpleInstruction("OP_FALSE", offset);
        case OP_TRUE:
            return simpleInstruction("OP_TRUE", offset);
        case OP_NULL:
            return simpleInstruction("OP_NULL", offset);
        case OP_NOT:
            return simpleInstruction("OP_NOT", offset);
        case OP_EQUAL:
            return simpleInstruction("OP_EQUAL", offset);
        case OP_GREATER:
            return simpleInstruction("OP_GREATER", offset);
        case OP_LESS:
            return simpleInstruction("OP_LESS", offset);
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
        case OP_DEFINE_GLOBAL:
            return constantInstruction("OP_DEFINE_GLOBAL", bytecodeChunk, offset);
        case OP_GET_GLOBAL:
            return constantInstruction("OP_GET_GLOBAL", bytecodeChunk, offset);
        case OP_SET_GLOBAL:
            return constantInstruction("OP_SET_GLOBAL", bytecodeChunk, offset);
        case OP_GET_LOCAL:
            return byteInstruction("OP_GET_LOCAL", bytecodeChunk, offset);
        case OP_SET_LOCAL:
            return byteInstruction("OP_SET_LOCAL", bytecodeChunk, offset);
        case OP_JUMP:
            return jumpInstruction("OP_JUMP", 1, bytecodeChunk, offset);
        case OP_JUMP_IF_FALSE:
            return jumpInstruction("OP_JUMP_IF_FALSE", 1, bytecodeChunk, offset);
        case OP_LOOP:
            return jumpInstruction("OP_LOOP", -1, bytecodeChunk, offset);
        case OP_CALL:
            return byteInstruction("OP_CALL", bytecodeChunk, offset);
        case OP_RETURN:
            return simpleInstruction("OP_RETURN", offset);
        case OP_POP:
            return simpleInstruction("OP_POP", offset);
        case OP_POP_N:
            return simpleInstruction("OP_POP_N", offset);
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
