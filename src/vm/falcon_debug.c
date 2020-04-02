/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * falcon_debug.c: List of debugging function for the Falcon Compiler/VM
 * See Falcon's license in the LICENSE file
 */

#include "falcon_debug.h"
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
 * Displays a byte instruction.
 */
static int byteInstruction(const char *name, BytecodeChunk *bytecode, int offset) {
    uint8_t slot = bytecode->code[offset + 1];
    printf("%-16s %4d\n", name, slot);
    return offset + 2;
}

/**
 * Displays a collection (list or maps) instruction.
 */
static int collectionInstruction(const char *name, BytecodeChunk *bytecode, int offset) {
    uint16_t length = (uint16_t)(bytecode->code[offset + 1] << 8u);
    length |= bytecode->code[offset + 2];
    printf("%-16s %4d\n", name, length);
    return offset + 3;
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
static int constantInstruction(const char *name, FalconVM *vm, BytecodeChunk *bytecode,
                               int offset) {
    uint8_t constant = bytecode->code[offset + 1];
    FalconValue value = bytecode->constants.values[constant];

    /* Prints the constant */
    printf("%-16s %4d '", name, constant);
    printValue(vm, value);
    printf("'\n");
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
    printf("%-16s %4d '", name, constant);
    printValue(vm, value);
    printf("'\n");
    return offset + 3;
}

/**
 * Displays a method invocation instruction.
 */
static int invokeInstruction(const char *name, FalconVM *vm, BytecodeChunk *bytecode, int offset) {
    uint8_t argCount = bytecode->code[offset + 1];
    uint8_t constant = bytecode->code[offset + 2];
    printf("%-19s %d %d '", name, argCount, constant);
    printValue(vm, bytecode->constants.values[constant]);
    printf("'\n");
    return offset + 3;
}

/**
 * Displays a closure instruction.
 */
static int closureInstruction(const char *name, FalconVM *vm, BytecodeChunk *bytecode, int offset) {
    offset++;
    uint8_t constant = bytecode->code[offset++];
    printf("%-16s %4d '", name, constant);
    printValue(vm, bytecode->constants.values[constant]);
    printf("'\n");

    ObjFunction *function = AS_FUNCTION(bytecode->constants.values[constant]);
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
int dumpInstruction(FalconVM *vm, BytecodeChunk *bytecode, int offset) {
    printf("%04d ", offset); /* Prints offset info */
    int sourceLine = getLine(bytecode, offset);

    /* Prints line info */
    if (offset > 0 && sourceLine == getLine(bytecode, offset - 1)) {
        printf("   | ");
    } else {
        printf("%4d ", sourceLine);
    }

    uint8_t instruction = bytecode->code[offset]; /* Current instruction */
    switch (instruction) {                        /* Verifies the instruction type */

        /* Constants and literals */
        case OP_LOADCONST:
            return constantInstruction16("LOADCONST", vm, bytecode, offset);
        case OP_LOADFALSE:
            return simpleInstruction("LOADFALSE", offset);
        case OP_LOADTRUE:
            return simpleInstruction("LOADTRUE", offset);
        case OP_LOADNULL:
            return simpleInstruction("LOADNULL", offset);

        /* Lists */
        case OP_DEFLIST:
            return collectionInstruction("DEFLIST", bytecode, offset);
        case OP_DEFMAP:
            return collectionInstruction("DEFMAP", bytecode, offset);
        case OP_GETSUB:
            return simpleInstruction("GETSUB", offset);
        case OP_SETSUB:
            return simpleInstruction("SETSUB", offset);

        /* Relational operations */
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

        /* Arithmetic operations */
        case OP_ADD:
            return simpleInstruction("ADD", offset);
        case OP_SUB:
            return simpleInstruction("SUB", offset);
        case OP_NEG:
            return simpleInstruction("NEG", offset);
        case OP_DIV:
            return simpleInstruction("DIV", offset);
        case OP_MOD:
            return simpleInstruction("MOD", offset);
        case OP_MULT:
            return simpleInstruction("MULT", offset);
        case OP_POW:
            return simpleInstruction("POW", offset);

        /* Variable operations */
        case OP_DEFGLOBAL:
            return constantInstruction("DEFGLOBAL", vm, bytecode, offset);
        case OP_GETGLOBAL:
            return constantInstruction("GETGLOBAL", vm, bytecode, offset);
        case OP_SETGLOBAL:
            return constantInstruction("SETGLOBAL", vm, bytecode, offset);
        case OP_GETUPVAL:
            return byteInstruction("GETUPVAL", bytecode, offset);
        case OP_SETUPVAL:
            return byteInstruction("SETUPVAL", bytecode, offset);
        case OP_CLOSEUPVAL:
            return simpleInstruction("CLOSEUPVAL", offset);
        case OP_GETLOCAL:
            return byteInstruction("GETLOCAL", bytecode, offset);
        case OP_SETLOCAL:
            return byteInstruction("SETLOCAL", bytecode, offset);

        /* Jump/loop operations */
        case OP_JUMP:
            return jumpInstruction("JUMP", 1, bytecode, offset);
        case OP_JUMPIFF:
            return jumpInstruction("JUMPIFF", 1, bytecode, offset);
        case OP_LOOP:
            return jumpInstruction("LOOP", -1, bytecode, offset);

        /* Closures/functions operations */
        case OP_CLOSURE:
            return closureInstruction("CLOSURE", vm, bytecode, offset);
        case OP_CALL:
            return byteInstruction("CALL", bytecode, offset);
        case OP_RETURN:
            return simpleInstruction("RETURN", offset);

        /* Class operations */
        case OP_DEFCLASS:
            return constantInstruction("DEFCLASS", vm, bytecode, offset);
        case OP_DEFMETHOD:
            return constantInstruction("DEFMETHOD", vm, bytecode, offset);
        case OP_GETPROP:
            return constantInstruction("GETPROP", vm, bytecode, offset);
        case OP_SETPROP:
            return constantInstruction("SETPROP", vm, bytecode, offset);
        case OP_INVPROP:
            return invokeInstruction("INVPROP", vm, bytecode, offset);

        /* VM operations */
        case OP_DUPT:
            return simpleInstruction("DUPT", offset);
        case OP_POPT:
            return simpleInstruction("POPT", offset);
        case OP_POPEXPR:
            return simpleInstruction("POPEXPR", offset);
        case OP_TEMP:
            return simpleInstruction("TEMP", offset); /* Should not be reachable */

        /* Unknown opcode */
        default:
            printf("Unknown opcode %d\n", instruction);
            return offset + 1;
    }
}

/**
 * Displays a complete bytecode chunk.
 */
void dumpBytecode(FalconVM *vm, BytecodeChunk *bytecode, const char *name) {
    printf("== %s ==\n", name);
    for (int offset = 0; offset < bytecode->count;) {   /* Loop through the instructions */
        offset = dumpInstruction(vm, bytecode, offset); /* Disassemble instruction */
    }
}

/**
 * Displays the Falcon's virtual machine stack.
 */
void dumpStack(FalconVM *vm) {
    printf("Stack:  ");
    for (FalconValue *slot = vm->stack; slot < vm->stackTop; slot++) {
        printf("[ '");
        printValue(vm, *slot);
        printf("' ] ");
    }
    printf("\n");
}

/**
 * Displays debug information on the allocation of a Falcon Object on the heap.
 */
void dumpAllocation(FalconObj *object, size_t size, ObjType type) {
    printf("Allocated %ld bytes for type \"%s\" at address %p\n", size, getObjName(type),
           (void *) object);
}

/**
 * Displays debug information on the free of a Falcon Object on the heap.
 */
void dumpFree(FalconObj *object) {
    printf("Freed object from type \"%s\" at address %p\n", getObjName(object->type),
           (void *) object);
}

/**
 * Displays the current status of the garbage collector.
 */
void dumpGCStatus(const char *status) { printf("== Garbage Collector %s ==\n", status); }

/**
 * Displays debug information on the "marking" of a Falcon Object for garbage collection.
 */
void dumpMark(FalconObj *object) { printf("Object at address %p marked\n", (void *) object); }

/**
 * Displays debug information on the "blacken" of a Falcon Object for garbage collection.
 */
void dumpBlacken(FalconObj *object) { printf("Object at address %p blackened\n", (void *) object); }

/**
 * Display the number of collected bytes after a garbage collection process, and the number of
 * bytes required for the next garbage collector activation.
 */
void dumpGC(FalconVM *vm, size_t bytesAllocated) {
    printf("Collected %ld bytes (from %ld to %ld)\n", bytesAllocated - vm->bytesAllocated,
           bytesAllocated, vm->bytesAllocated);
    printf("Next GC at %ld bytes\n", vm->nextGC);
}
