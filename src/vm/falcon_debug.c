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
void falconOpcodesHeader() {
    printf("=============================================================\n");
    printf("================= DEBUGGING - PRINT OPCODES =================\n");
    printf("=============================================================\n");
}

/**
 * Displays the debugging (trace execution) header.
 */
void falconExecutionHeader() {
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
static int constantInstruction(const char *name, BytecodeChunk *bytecode, int offset) {
    uint8_t constant = bytecode->code[offset + 1];
    FalconValue value = bytecode->constants.values[constant];

    /* Prints the constant */
    printf("%-16s %4d ", name, constant);
    printf("'");
    falconPrintVal(value);
    printf("'");
    printf("\n");
    return offset + 2;
}

/**
 * Displays a constant instruction (16 bits).
 */
static int constantInstruction16(const char *name, BytecodeChunk *bytecode, int offset) {
    uint16_t constant = (bytecode->code[offset + 1] | (uint16_t)(bytecode->code[offset + 2] << 8u));
    FalconValue value = bytecode->constants.values[constant];

    /* Prints the constant */
    printf("%-16s %4d ", name, constant);
    printf("'");
    falconPrintVal(value);
    printf("'");
    printf("\n");
    return offset + 3;
}

/**
 * Displays a closure instruction.
 */
static int closureInstruction(const char *name, BytecodeChunk *bytecode, int offset) {
    offset++;
    uint8_t constant = bytecode->code[offset++];
    printf("%-16s %4d ", name, constant);
    falconPrintVal(bytecode->constants.values[constant]);
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
int falconDumpInstruction(BytecodeChunk *bytecode, int offset) {
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
        case OP_CONSTANT:
            return constantInstruction16("CONSTANT", bytecode, offset);
        case OP_FALSE_LIT:
            return simpleInstruction("FALSE_LIT", offset);
        case OP_TRUE_LIT:
            return simpleInstruction("TRUE_LIT", offset);
        case OP_NULL_LIT:
            return simpleInstruction("NULL_LIT", offset);
        case OP_AND:
            return simpleInstruction("AND", offset);
        case OP_OR:
            return simpleInstruction("OR", offset);
        case OP_NOT:
            return simpleInstruction("NOT", offset);
        case OP_EQUAL:
            return simpleInstruction("EQUAL", offset);
        case OP_GREATER:
            return simpleInstruction("GREATER", offset);
        case OP_LESS:
            return simpleInstruction("LESS", offset);
        case OP_ADD:
            return simpleInstruction("ADD", offset);
        case OP_SUBTRACT:
            return simpleInstruction("SUBTRACT", offset);
        case OP_NEGATE:
            return simpleInstruction("NEGATE", offset);
        case OP_DIVIDE:
            return simpleInstruction("DIVIDE", offset);
        case OP_MOD:
            return simpleInstruction("MOD", offset);
        case OP_MULTIPLY:
            return simpleInstruction("MULTIPLY", offset);
        case OP_POW:
            return simpleInstruction("POW", offset);
        case OP_DEFINE_GLOBAL:
            return constantInstruction("DEFINE_GLOBAL", bytecode, offset);
        case OP_GET_GLOBAL:
            return constantInstruction("GET_GLOBAL", bytecode, offset);
        case OP_SET_GLOBAL:
            return constantInstruction("SET_GLOBAL", bytecode, offset);
        case OP_GET_UPVALUE:
            return byteInstruction("GET_UPVALUE", bytecode, offset);
        case OP_SET_UPVALUE:
            return byteInstruction("SET_UPVALUE", bytecode, offset);
        case OP_CLOSE_UPVALUE:
            return simpleInstruction("CLOSE_UPVALUE", offset);
        case OP_GET_LOCAL:
            return byteInstruction("GET_LOCAL", bytecode, offset);
        case OP_SET_LOCAL:
            return byteInstruction("SET_LOCAL", bytecode, offset);
        case OP_JUMP:
            return jumpInstruction("JUMP", 1, bytecode, offset);
        case OP_JUMP_IF_FALSE:
            return jumpInstruction("JUMP_IF_FALSE", 1, bytecode, offset);
        case OP_LOOP:
            return jumpInstruction("LOOP", -1, bytecode, offset);
        case OP_CLOSURE:
            return closureInstruction("CLOSURE", bytecode, offset);
        case OP_CALL:
            return byteInstruction("CALL", bytecode, offset);
        case OP_RETURN:
            return simpleInstruction("RETURN", offset);
        case OP_DUP:
            return simpleInstruction("DUP", offset);
        case OP_POP:
            return simpleInstruction("POP", offset);
        case OP_POP_EXPR:
            return simpleInstruction("POP_EXPR", offset);
        case OP_TEMP:
            return simpleInstruction("TEMP", offset); /* Should not be reachable */
        default:
            printf("Unknown opcode %d\n", instruction);
            return offset + 1;
    }
}

/**
 * Displays a complete bytecode chunk.
 */
void falconDumpBytecode(BytecodeChunk *bytecode, const char *name) {
    printf("== %s ==\n", name);
    for (int offset = 0; offset < bytecode->count;) {     /* Loop through the instructions */
        offset = falconDumpInstruction(bytecode, offset); /* Disassemble instruction */
    }
}

/**
 * Displays the Falcon's virtual machine stack.
 */
void falconDumpStack(FalconVM *vm) {
    printf("Stack: ");
    for (FalconValue *slot = vm->stack; slot < vm->stackTop; slot++) {
        printf("[ ");
        falconPrintVal(*slot);
        printf(" ] ");
    }
    printf("\nStack count: %ld\n", vm->stackTop - &vm->stack[0]);
}

/**
 * Displays debug information on the allocation of a Falcon Object on the heap.
 */
void falconDumpAllocation(FalconObj *object, size_t size, ObjType type) {
    printf("%p allocated %ld bytes for type \"%s\"\n", (void *) object, size,
           falconGetObjName(type));
}

/**
 * Displays debug information on the free of a Falcon Object on the heap.
 */
void falconDumpFree(FalconObj *object) {
    printf("%p freed object from type \"%s\"\n", (void *) object, falconGetObjName(object->type));
}

/**
 * Displays the current status of the garbage collector.
 */
void falconGCStatus(const char *status) { printf("== Garbage Collector %s ==\n", status); }

/**
 * Displays debug information on the "marking" of a Falcon Object for garbage collection.
 */
void falconDumpMark(FalconObj *object) {
    printf("%p marked ", (void *) object);
    falconPrintVal(FALCON_OBJ_VAL(object));
    printf("\n");
}

/**
 * Displays debug information on the "blacken" of a Falcon Object for garbage collection.
 */
void falconDumpBlacken(FalconObj *object) {
    printf("%p blackened ", (void *) object);
    falconPrintVal(FALCON_OBJ_VAL(object));
    printf("\n");
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
