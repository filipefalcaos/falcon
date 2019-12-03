/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * falcon_debug.c: List of debugging function for the Falcon Compiler/VM
 * See Falcon's license in the LICENSE file
 */

#include "falcon_debug.h"
#include "../vm/falcon_value.h"
#include "../vm/falcon_vm.h"
#include <stdio.h>

/**
 * Print debugging (opcodes) header.
 */
void FalconOpcodesHeader() {
    printf("=============================================================\n");
    printf("================= DEBUGGING - PRINT OPCODES =================\n");
    printf("=============================================================\n");
}

/**
 * Print debugging (trace execution) header.
 */
void FalconExecutionHeader() {
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
static int byteInstruction(const char *name, FalconBytecodeChunk *bytecodeChunk, int offset) {
    uint8_t slot = bytecodeChunk->code[offset + 1];
    printf("%-16s %4d\n", name, slot);
    return offset + 2;
}

/**
 * Displays a jump (conditional) instruction.
 */
static int jumpInstruction(const char *name, int sign, FalconBytecodeChunk *bytecodeChunk, int offset) {
    uint16_t jump = (uint16_t) (bytecodeChunk->code[offset + 1] << 8);
    jump |= bytecodeChunk->code[offset + 2];
    printf("%-16s %4d -> %d\n", name, offset, offset + 3 + sign * jump);
    return offset + 3;
}

/**
 * Displays a constant instruction (8 bits).
 */
static int constantInstruction(const char *name, FalconBytecodeChunk *bytecodeChunk, int offset) {
    uint8_t constant = bytecodeChunk->code[offset + 1];
    FalconValue value = bytecodeChunk->constants.values[constant];

    /* Prints the constant */
    printf("%-16s %4d ", name, constant);
    printf("'");
    FalconPrintValue(value);
    printf("'");
    printf("\n");
    return offset + 2;
}

/**
 * Displays a constant instruction (16 bits).
 */
static int constantInstruction16(const char *name, FalconBytecodeChunk *bytecodeChunk, int offset) {
    uint16_t constant = bytecodeChunk->code[offset + 1] | (bytecodeChunk->code[offset + 2] << 8);
    FalconValue value = bytecodeChunk->constants.values[constant];

    /* Prints the constant */
    printf("%-16s %4d ", name, constant);
    printf("'");
    FalconPrintValue(value);
    printf("'");
    printf("\n");
    return offset + 3;
}

/**
 * Displays a closure instruction.
 */
static int closureInstruction(const char *name, FalconBytecodeChunk *bytecodeChunk, int offset) {
    offset++;
    uint8_t constant = bytecodeChunk->code[offset++];
    printf("%-16s %4d ", name, constant);
    FalconPrintValue(bytecodeChunk->constants.values[constant]);
    printf("\n");

    FalconObjFunction *function = FALCON_AS_FUNCTION(bytecodeChunk->constants.values[constant]);
    for (int i = 0; i < function->upvalueCount; i++) {
        int isLocal = bytecodeChunk->code[offset++];
        int index = bytecodeChunk->code[offset++];
        printf("%04d    |                     %s %d\n", offset - 2, isLocal ? "local" : "upvalue",
               index);
    }

    return offset;
}

/**
 * Disassembles a single instruction in a bytecode chunk.
 */
int FalconDisassembleInst(FalconBytecodeChunk *bytecodeChunk, int offset) {
    printf("%04d ", offset); /* Prints offset info */
    int sourceLine = FalconGetLine(bytecodeChunk, offset);

    /* Prints line info */
    if (offset > 0 && sourceLine == FalconGetLine(bytecodeChunk, offset - 1)) {
        printf("   | ");
    } else {
        printf("%4d ", sourceLine);
    }

    uint8_t instruction = bytecodeChunk->code[offset]; /* Current instruction */
    switch (instruction) {                             /* Verifies the instruction type */
        case FALCON_OP_CONSTANT:
            return constantInstruction16("OP_CONSTANT", bytecodeChunk, offset);
        case FALCON_OP_FALSE:
            return simpleInstruction("OP_FALSE", offset);
        case FALCON_OP_TRUE:
            return simpleInstruction("OP_TRUE", offset);
        case FALCON_OP_NULL:
            return simpleInstruction("OP_NULL", offset);
        case FALCON_OP_AND:
            return simpleInstruction("OP_AND", offset);
        case FALCON_OP_OR:
            return simpleInstruction("OP_OR", offset);
        case FALCON_OP_NOT:
            return simpleInstruction("OP_NOT", offset);
        case FALCON_OP_EQUAL:
            return simpleInstruction("OP_EQUAL", offset);
        case FALCON_OP_GREATER:
            return simpleInstruction("OP_GREATER", offset);
        case FALCON_OP_LESS:
            return simpleInstruction("OP_LESS", offset);
        case FALCON_OP_ADD:
            return simpleInstruction("OP_ADD", offset);
        case FALCON_OP_SUBTRACT:
            return simpleInstruction("OP_SUBTRACT", offset);
        case FALCON_OP_NEGATE:
            return simpleInstruction("OP_NEGATE", offset);
        case FALCON_OP_MULTIPLY:
            return simpleInstruction("OP_MULTIPLY", offset);
        case FALCON_OP_MOD:
            return simpleInstruction("OP_MOD", offset);
        case FALCON_OP_DIVIDE:
            return simpleInstruction("OP_DIVIDE", offset);
        case FALCON_OP_DEFINE_GLOBAL:
            return constantInstruction("OP_DEFINE_GLOBAL", bytecodeChunk, offset);
        case FALCON_OP_GET_GLOBAL:
            return constantInstruction("OP_GET_GLOBAL", bytecodeChunk, offset);
        case FALCON_OP_SET_GLOBAL:
            return constantInstruction("OP_SET_GLOBAL", bytecodeChunk, offset);
        case FALCON_OP_GET_UPVALUE:
            return byteInstruction("OP_GET_UPVALUE", bytecodeChunk, offset);
        case FALCON_OP_SET_UPVALUE:
            return byteInstruction("OP_SET_UPVALUE", bytecodeChunk, offset);
        case FALCON_OP_CLOSE_UPVALUE:
            return simpleInstruction("OP_CLOSE_UPVALUE", offset);
        case FALCON_OP_GET_LOCAL:
            return byteInstruction("OP_GET_LOCAL", bytecodeChunk, offset);
        case FALCON_OP_SET_LOCAL:
            return byteInstruction("OP_SET_LOCAL", bytecodeChunk, offset);
        case FALCON_OP_JUMP:
            return jumpInstruction("OP_JUMP", 1, bytecodeChunk, offset);
        case FALCON_OP_JUMP_IF_FALSE:
            return jumpInstruction("OP_JUMP_IF_FALSE", 1, bytecodeChunk, offset);
        case FALCON_OP_LOOP:
            return jumpInstruction("OP_LOOP", -1, bytecodeChunk, offset);
        case FALCON_OP_CLOSURE:
            return closureInstruction("OP_CLOSURE", bytecodeChunk, offset);
        case FALCON_OP_CALL:
            return byteInstruction("OP_CALL", bytecodeChunk, offset);
        case FALCON_OP_RETURN:
            return simpleInstruction("OP_RETURN", offset);
        case FALCON_OP_DUP:
            return simpleInstruction("OP_DUP", offset);
        case FALCON_OP_POP:
            return simpleInstruction("OP_POP", offset);
        default:
            printf("Unknown opcode %d\n", instruction);
            return offset + 1;
    }
}

/**
 * Disassembles a complete bytecode chunk.
 */
void FalconDisassembleBytecode(FalconBytecodeChunk *bytecodeChunk, const char *name) {
    printf("== %s ==\n", name);

    for (int offset = 0; offset < bytecodeChunk->count;) { /* Loop through the instructions */
        offset = FalconDisassembleInst(bytecodeChunk, offset); /* Disassemble instruction */
    }
}
