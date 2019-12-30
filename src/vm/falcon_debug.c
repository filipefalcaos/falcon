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
 * Displays a simple bytecode instruction.
 */
static int simpleInstruction(const char *name, int offset) {
    printf("%s\n", name);
    return offset + 1;
}

/**
 * Displays a local variable instruction.
 */
static int byteInstruction(const char *name, BytecodeChunk *bytecode, int offset) {
    uint8_t slot = bytecode->code[offset + 1];
    printf("%-16s %4d\n", name, slot);
    return offset + 2;
}

/**
 * Displays a jump (conditional) instruction.
 */
static int jumpInstruction(const char *name, int sign, BytecodeChunk *bytecode, int offset) {
    uint16_t jump = (uint16_t)(bytecode->code[offset + 1] << 8u);
    jump |= bytecode->code[offset + 2];
    printf("%-16s %4d -> %d\n", name, offset, offset + 3 + sign * jump);
    return offset + 3;
}

/**
 * Displays a constant instruction (8 bits).
 */
static int constantInstruction(const char *name, FalconVM *vm, BytecodeChunk *bytecode, int offset) {
    uint8_t constant = bytecode->code[offset + 1];
    FalconValue value = bytecode->constants.values[constant];

    /* Prints the constant */
    printf("%-16s %4d ", name, constant);
    falconPrintVal(vm, value, true);
    printf("\n");
    return offset + 2;
}

/**
 * Displays a constant instruction (16 bits).
 */
static int constantInstruction16(const char *name, FalconVM *vm, BytecodeChunk *bytecode,
                                 int offset) {
    uint16_t constant = (bytecode->code[offset + 1] | (uint16_t)(bytecode->code[offset + 2] << 8u));
    FalconValue value = bytecode->constants.values[constant];

    /* Prints the constant */
    printf("%-16s %4d ", name, constant);
    falconPrintVal(vm, value, true);
    printf("\n");
    return offset + 3;
}

/**
 * Displays a closure instruction.
 */
static int closureInstruction(const char *name, FalconVM *vm, BytecodeChunk *bytecode, int offset) {
    offset++;
    uint8_t constant = bytecode->code[offset++];
    printf("%-16s %4d ", name, constant);
    falconPrintVal(vm, bytecode->constants.values[constant], false);
    printf("\n");

    ObjFunction *function = FALCON_AS_FUNCTION(bytecode->constants.values[constant]);
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
int falconDumpInstruction(FalconVM *vm, BytecodeChunk *bytecode, int offset) {
    printf("%04d ", offset); /* Prints offset info */
    int sourceLine = falconGetLine(bytecode, offset);

    /* Prints line info */
    if (offset > 0 && sourceLine == falconGetLine(bytecode, offset - 1)) {
        printf("   | ");
    } else {
        printf("%4d ", sourceLine);
    }

    uint8_t instruction = bytecode->code[offset]; /* Current instruction */
    switch (instruction) {                        /* Verifies the instruction type */
        case LOAD_CONST:
            return constantInstruction16("LOAD_CONST", vm, bytecode, offset);
        case LOAD_FALSE:
            return simpleInstruction("LOAD_FALSE", offset);
        case LOAD_TRUE:
            return simpleInstruction("LOAD_TRUE", offset);
        case LOAD_NULL:
            return simpleInstruction("LOAD_NULL", offset);
        case DEF_LIST:
            return byteInstruction("DEF_LIST", bytecode, offset);
        case PUSH_LIST:
            return simpleInstruction("PUSH_LIST", offset);
        case GET_IDX_LIST:
            return simpleInstruction("GET_IDX_LIST", offset);
        case SET_IDX_LIST:
            return simpleInstruction("SET_IDX_LIST", offset);
        case BIN_AND:
            return simpleInstruction("BIN_AND", offset);
        case BIN_OR:
            return simpleInstruction("BIN_OR", offset);
        case UN_NOT:
            return simpleInstruction("UN_NOT", offset);
        case BIN_EQUAL:
            return simpleInstruction("BIN_EQUAL", offset);
        case BIN_GREATER:
            return simpleInstruction("BIN_GREATER", offset);
        case BIN_LESS:
            return simpleInstruction("BIN_LESS", offset);
        case BIN_ADD:
            return simpleInstruction("BIN_ADD", offset);
        case BIN_SUB:
            return simpleInstruction("BIN_SUB", offset);
        case UN_NEG:
            return simpleInstruction("UN_NEG", offset);
        case BIN_DIV:
            return simpleInstruction("BIN_DIV", offset);
        case BIN_MOD:
            return simpleInstruction("BIN_MOD", offset);
        case BIN_MULT:
            return simpleInstruction("BIN_MULT", offset);
        case BIN_POW:
            return simpleInstruction("BIN_POW", offset);
        case DEF_GLOBAL:
            return constantInstruction("DEF_GLOBAL", vm, bytecode, offset);
        case GET_GLOBAL:
            return constantInstruction("GET_GLOBAL", vm, bytecode, offset);
        case SET_GLOBAL:
            return constantInstruction("SET_GLOBAL", vm, bytecode, offset);
        case GET_UPVALUE:
            return byteInstruction("GET_UPVALUE", bytecode, offset);
        case SET_UPVALUE:
            return byteInstruction("SET_UPVALUE", bytecode, offset);
        case CLS_UPVALUE:
            return simpleInstruction("CLS_UPVALUE", offset);
        case GET_LOCAL:
            return byteInstruction("GET_LOCAL", bytecode, offset);
        case SET_LOCAL:
            return byteInstruction("SET_LOCAL", bytecode, offset);
        case JUMP_FWR:
            return jumpInstruction("JUMP_FWR", 1, bytecode, offset);
        case JUMP_IF_FALSE:
            return jumpInstruction("JUMP_IF_FALSE", 1, bytecode, offset);
        case LOOP_BACK:
            return jumpInstruction("LOOP_BACK", -1, bytecode, offset);
        case FN_CLOSURE:
            return closureInstruction("FN_CLOSURE", vm, bytecode, offset);
        case FN_CALL:
            return byteInstruction("FN_CALL", bytecode, offset);
        case FN_RETURN:
            return simpleInstruction("FN_RETURN", offset);
        case DUP_TOP:
            return simpleInstruction("DUP_TOP", offset);
        case POP_TOP:
            return simpleInstruction("POP_TOP", offset);
        case POP_TOP_EXPR:
            return simpleInstruction("POP_TOP_EXPR", offset);
        case TEMP_MARK:
            return simpleInstruction("TEMP_MARK", offset); /* Should not be reachable */
        default:
            printf("Unknown opcode %d\n", instruction);
            return offset + 1;
    }
}

/**
 * Displays a complete bytecode chunk.
 */
void falconDumpBytecode(FalconVM *vm, BytecodeChunk *bytecode, const char *name) {
    printf("== %s ==\n", name);
    for (int offset = 0; offset < bytecode->count;) {         /* Loop through the instructions */
        offset = falconDumpInstruction(vm, bytecode, offset); /* Disassemble instruction */
    }
}

/**
 * Displays the Falcon's virtual machine stack.
 */
void falconDumpStack(FalconVM *vm) {
    printf("Stack:  ");
    for (FalconValue *slot = vm->stack; slot < vm->stackTop; slot++) {
        printf("[ ");
        falconPrintVal(vm, *slot, false);
        printf(" ] ");
    }
    printf("\n");
}

/**
 * Displays debug information on the allocation of a Falcon Object on the heap.
 */
void falconDumpAllocation(FalconObj *object, size_t size, ObjType type) {
    printf("Allocated %ld bytes for type \"%s\" at address %p\n", size, falconGetObjName(type),
           (void *) object);
}

/**
 * Displays debug information on the free of a Falcon Object on the heap.
 */
void falconDumpFree(FalconObj *object) {
    printf("Freed object from type \"%s\" at address %p\n", falconGetObjName(object->type),
           (void *) object);
}

/**
 * Displays the current status of the garbage collector.
 */
void falconGCStatus(const char *status) { printf("== Garbage Collector %s ==\n", status); }

/**
 * Displays debug information on the "marking" of a Falcon Object for garbage collection.
 */
void falconDumpMark(FalconObj *object) {
    printf("Object at address %p marked\n", (void *) object);
}

/**
 * Displays debug information on the "blacken" of a Falcon Object for garbage collection.
 */
void falconDumpBlacken(FalconObj *object) {
    printf("Object at address %p blackened\n", (void *) object);
}

/**
 * Display the number of collected bytes after a garbage collection process, and the number of
 * bytes required for the next garbage collector activation.
 */
void falconDumpGC(FalconVM *vm, size_t bytesAllocated) {
    printf("Collected %ld bytes (from %ld to %ld)\n", bytesAllocated - vm->bytesAllocated,
           bytesAllocated, vm->bytesAllocated);
    printf("Next GC at %ld bytes\n", vm->nextGC);
}
