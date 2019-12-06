/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * falcon_debug.c: List of debugging function for the Falcon Compiler/VM
 * See Falcon's license in the LICENSE file
 */

#include "falcon_debug.h"
#include "falcon_value.h"
#include "falcon_vm.h"
#include <stdio.h>

/**
 * Displays the debugging (opcodes) header.
 */
void FalconOpcodesHeader() {
    printf("=============================================================\n");
    printf("================= DEBUGGING - PRINT OPCODES =================\n");
    printf("=============================================================\n");
}

/**
 * Displays the debugging (trace execution) header.
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
static int byteInstruction(const char *name, FalconBytecodeChunk *bytecode, int offset) {
    uint8_t slot = bytecode->code[offset + 1];
    printf("%-16s %4d\n", name, slot);
    return offset + 2;
}

/**
 * Displays a jump (conditional) instruction.
 */
static int jumpInstruction(const char *name, int sign, FalconBytecodeChunk *bytecode, int offset) {
    uint16_t jump = (uint16_t) (bytecode->code[offset + 1] << 8u);
    jump |= bytecode->code[offset + 2];
    printf("%-16s %4d -> %d\n", name, offset, offset + 3 + sign * jump);
    return offset + 3;
}

/**
 * Displays a constant instruction (8 bits).
 */
static int constantInstruction(const char *name, FalconBytecodeChunk *bytecode, int offset) {
    uint8_t constant = bytecode->code[offset + 1];
    FalconValue value = bytecode->constants.values[constant];

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
static int constantInstruction16(const char *name, FalconBytecodeChunk *bytecode, int offset) {
    uint16_t constant = (bytecode->code[offset + 1] | (uint16_t)(bytecode->code[offset + 2] << 8u));
    FalconValue value = bytecode->constants.values[constant];

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
static int closureInstruction(const char *name, FalconBytecodeChunk *bytecode, int offset) {
    offset++;
    uint8_t constant = bytecode->code[offset++];
    printf("%-16s %4d ", name, constant);
    FalconPrintValue(bytecode->constants.values[constant]);
    printf("\n");

    FalconObjFunction *function = FALCON_AS_FUNCTION(bytecode->constants.values[constant]);
    for (int i = 0; i < function->upvalueCount; i++) {
        int isLocal = bytecode->code[offset++];
        int index = bytecode->code[offset++];
        printf("%04d    |                     %s %d\n", offset - 2, isLocal ? "local" : "upvalue",
               index);
    }

    return offset;
}

/**
 * Displays a single instruction in a bytecode chunk.
 */
int FalconDumpInstruction(FalconBytecodeChunk *bytecode, int offset) {
    printf("%04d ", offset); /* Prints offset info */
    int sourceLine = FalconGetLine(bytecode, offset);

    /* Prints line info */
    if (offset > 0 && sourceLine == FalconGetLine(bytecode, offset - 1)) {
        printf("   | ");
    } else {
        printf("%4d ", sourceLine);
    }

    uint8_t instruction = bytecode->code[offset]; /* Current instruction */
    switch (instruction) {                        /* Verifies the instruction type */
        case FALCON_OP_CONSTANT:
            return constantInstruction16("OP_CONSTANT", bytecode, offset);
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
        case FALCON_OP_DIVIDE:
            return simpleInstruction("OP_DIVIDE", offset);
        case FALCON_OP_MOD:
            return simpleInstruction("OP_MOD", offset);
        case FALCON_OP_MULTIPLY:
            return simpleInstruction("OP_MULTIPLY", offset);
        case FALCON_OP_POW:
            return simpleInstruction("FALCON_OP_POW", offset);
        case FALCON_OP_DEFINE_GLOBAL:
            return constantInstruction("OP_DEFINE_GLOBAL", bytecode, offset);
        case FALCON_OP_GET_GLOBAL:
            return constantInstruction("OP_GET_GLOBAL", bytecode, offset);
        case FALCON_OP_SET_GLOBAL:
            return constantInstruction("OP_SET_GLOBAL", bytecode, offset);
        case FALCON_OP_GET_UPVALUE:
            return byteInstruction("OP_GET_UPVALUE", bytecode, offset);
        case FALCON_OP_SET_UPVALUE:
            return byteInstruction("OP_SET_UPVALUE", bytecode, offset);
        case FALCON_OP_CLOSE_UPVALUE:
            return simpleInstruction("OP_CLOSE_UPVALUE", offset);
        case FALCON_OP_GET_LOCAL:
            return byteInstruction("OP_GET_LOCAL", bytecode, offset);
        case FALCON_OP_SET_LOCAL:
            return byteInstruction("OP_SET_LOCAL", bytecode, offset);
        case FALCON_OP_JUMP:
            return jumpInstruction("OP_JUMP", 1, bytecode, offset);
        case FALCON_OP_JUMP_IF_FALSE:
            return jumpInstruction("OP_JUMP_IF_FALSE", 1, bytecode, offset);
        case FALCON_OP_LOOP:
            return jumpInstruction("OP_LOOP", -1, bytecode, offset);
        case FALCON_OP_CLOSURE:
            return closureInstruction("OP_CLOSURE", bytecode, offset);
        case FALCON_OP_CALL:
            return byteInstruction("OP_CALL", bytecode, offset);
        case FALCON_OP_RETURN:
            return simpleInstruction("OP_RETURN", offset);
        case FALCON_OP_DUP:
            return simpleInstruction("OP_DUP", offset);
        case FALCON_OP_POP:
            return simpleInstruction("OP_POP", offset);
        case FALCON_OP_POP_EXPR:
            return simpleInstruction("FALCON_OP_POP_EXPR", offset);
        case FALCON_OP_TEMP:
            return simpleInstruction("FALCON_OP_TEMP", offset); /* Should not be reachable */
        default:
            printf("Unknown opcode %d\n", instruction);
            return offset + 1;
    }
}

/**
 * Displays a complete bytecode chunk.
 */
void FalconDumpBytecode(FalconBytecodeChunk *bytecode, const char *name) {
    printf("== %s ==\n", name);
    for (int offset = 0; offset < bytecode->count;) { /* Loop through the instructions */
        offset = FalconDumpInstruction(bytecode, offset); /* Disassemble instruction */
    }
}

/**
 * Displays the Falcon's virtual machine stack.
 */
void FalconDumpStack(FalconVM *vm) {
    printf("Stack: ");
    for (FalconValue *slot = vm->stack; slot < vm->stackTop; slot++) {
        printf("[ ");
        FalconPrintValue(*slot);
        printf(" ] ");
    }
    printf("\nStack count: %ld\n", vm->stackTop - &vm->stack[0]);
}

/**
 * Displays debug information on the allocation of a Falcon Object on the heap.
 */
void FalconDumpAllocation(FalconObj *object, size_t size, FalconObjType type) {
    printf("%p allocated %ld bytes for type \"%s\"\n", (void *) object, size, getObjectName(type));
}

/**
 * Displays debug information on the free of a Falcon Object on the heap.
 */
void FalconDumpFree(FalconObj *object) {
    printf("%p freed object from type \"%s\"\n", (void *) object, getObjectName(object->type));
}

/**
 * Displays the current status of the garbage collector.
 */
void FalconGCStatus(const char *status) {
    printf("== Garbage Collector %s ==\n", status);
}

/**
 * Displays debug information on the "marking" of a Falcon Object for garbage collection.
 */
void FalconDumpMark(FalconObj *object) {
    printf("%p marked ", (void *) object);
    FalconPrintValue(FALCON_OBJ_VAL(object));
    printf("\n");
}

/**
 * Displays debug information on the "blacken" of a Falcon Object for garbage collection.
 */
void FalconDumpBlacken(FalconObj *object) {
    printf("%p blackened ", (void *) object);
    FalconPrintValue(FALCON_OBJ_VAL(object));
    printf("\n");
}

/**
 * Display the number of collected bytes after a garbage collection process, and the number of
 * bytes required for the next garbage collector activation.
 */
void FalconDumpGC(FalconVM *vm, size_t bytesAllocated) {
    printf("Collected %ld bytes (from %ld to %ld)\n", bytesAllocated - vm->bytesAllocated,
           bytesAllocated, vm->bytesAllocated);
    printf("Next GC at %ld bytes\n", vm->nextGC);
}
