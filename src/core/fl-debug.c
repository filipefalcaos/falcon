/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * fl-debug.c: List of debugging function for the Falcon Compiler/VM
 * See Falcon's license in the LICENSE file
 */

#include "fl-debug.h"
#include "fl-vm.h"
#include <stdio.h>

/**
 * Prints to stdout a instruction that has no arguments.
 */
static int simple_instruction(const char *name, int offset) {
    printf("%s\n", name);
    return offset + 1;
}

/**
 * Prints to stdout a instruction that has a single byte argument.
 */
static int byte_instruction(const char *name, const BytecodeChunk *bytecode, int offset) {
    uint8_t slot = bytecode->code[offset + 1];
    printf("%-16s %4d\n", name, slot);
    return offset + 2;
}

/**
 * Prints to stdout a collection ("OP_DEFLIST" or "OP_DEFMAP") instruction.
 */
static int collection_instruction(const char *name, const BytecodeChunk *bytecode, int offset) {
    uint16_t length = (uint16_t)(bytecode->code[offset + 1] << 8u);
    length |= bytecode->code[offset + 2];
    printf("%-16s %4d\n", name, length);
    return offset + 3;
}

/**
 * Prints to stdout a jump instruction. The given sign defines if the jump will be forward or
 * backwards.
 */
static int jump_instruction(const char *name, int sign, const BytecodeChunk *bytecode, int offset) {
    uint16_t jump = (uint16_t)(bytecode->code[offset + 1] << 8u);
    jump |= bytecode->code[offset + 2];
    printf("%-16s %4d -> %d\n", name, offset, offset + 3 + sign * jump);
    return offset + 3;
}

/**
 * Prints to stdout a instruction that loads a constant from the constants table (constant index
 * has 8 bits).
 */
static int constant_instruction(const char *name, FalconVM *vm, const BytecodeChunk *bytecode,
                                int offset) {
    uint8_t constant = bytecode->code[offset + 1];
    FalconValue value = bytecode->constants.values[constant];

    /* Prints the constant */
    printf("%-16s %4d ", name, constant);
    print_value(vm, value);
    printf("\n");
    return offset + 2;
}

/**
 * Prints to stdout a "OP_LOADCONST" instruction (constant index has 16 bits).
 */
static int constant_instruction_16(const char *name, FalconVM *vm, const BytecodeChunk *bytecode,
                                   int offset) {
    uint16_t constant = (bytecode->code[offset + 1] | (uint16_t)(bytecode->code[offset + 2] << 8u));
    FalconValue value = bytecode->constants.values[constant];

    /* Prints the constant */
    printf("%-16s %4d ", name, constant);
    print_value(vm, value);
    printf("\n");
    return offset + 3;
}

/**
 * Prints to stdout a method invocation ("OP_INVPROP" or "OP_INVSUPER") instruction.
 */
static int invoke_instruction(const char *name, FalconVM *vm, const BytecodeChunk *bytecode,
                              int offset) {
    uint8_t constant = bytecode->code[offset + 1];
    uint8_t argCount = bytecode->code[offset + 2];
    printf("%-19s %d %d ", name, argCount, constant);
    print_value(vm, bytecode->constants.values[constant]);
    printf("\n");
    return offset + 3;
}

/**
 * Prints to stdout a "OP_CLOSURE" instruction.
 */
static int closure_instruction(const char *name, FalconVM *vm, const BytecodeChunk *bytecode,
                               int offset) {
    offset++;
    uint8_t constant = bytecode->code[offset++];
    printf("%-16s %4d ", name, constant);
    print_value(vm, bytecode->constants.values[constant]);
    printf("\n");

    ObjFunction *function = AS_FUNCTION(bytecode->constants.values[constant]);
    for (int i = 0; i < function->upvalueCount; i++) {
        int isLocal = bytecode->code[offset++];
        int index = bytecode->code[offset++];
        const int spacePadding = 19;
        printf("%04d    | %*c %s %d\n", offset - 2, spacePadding, ' ',
               isLocal ? "local" : "upvalue", index);
    }

    return offset;
}

/**
 * Prints to stdout a single instruction from a given bytecode chunk.
 * TODO: check if all opcodes are covered.
 */
int dump_instruction(FalconVM *vm, const BytecodeChunk *bytecode, int offset) {
    int sourceLine = get_source_line(bytecode, offset);

    /* Prints line info */
    if (offset > 0 && sourceLine == get_source_line(bytecode, offset - 1)) {
        printf("    ");
    } else {
        printf("%04d", sourceLine);
    }

    printf("    %04d    ", offset); /* Prints offset info */

    uint8_t instruction = bytecode->code[offset]; /* Current instruction */
    switch (instruction) {                        /* Verifies the instruction type */

        /* Constants and literals */
        case OP_LOADCONST: return constant_instruction_16("LOADCONST", vm, bytecode, offset);
        case OP_LOADFALSE: return simple_instruction("LOADFALSE", offset);
        case OP_LOADTRUE: return simple_instruction("LOADTRUE", offset);
        case OP_LOADNULL: return simple_instruction("LOADNULL", offset);

        /* Lists */
        case OP_DEFLIST: return collection_instruction("DEFLIST", bytecode, offset);
        case OP_DEFMAP: return collection_instruction("DEFMAP", bytecode, offset);
        case OP_GETSUB: return simple_instruction("GETSUB", offset);
        case OP_SETSUB: return simple_instruction("SETSUB", offset);

        /* Relational operations */
        case OP_AND: return simple_instruction("AND", offset);
        case OP_OR: return simple_instruction("OR", offset);
        case OP_NOT: return simple_instruction("NOT", offset);
        case OP_EQUAL: return simple_instruction("EQUAL", offset);
        case OP_GREATER: return simple_instruction("GREATER", offset);
        case OP_LESS: return simple_instruction("LESS", offset);

        /* Arithmetic operations */
        case OP_ADD: return simple_instruction("ADD", offset);
        case OP_SUB: return simple_instruction("SUB", offset);
        case OP_NEG: return simple_instruction("NEG", offset);
        case OP_DIV: return simple_instruction("DIV", offset);
        case OP_MOD: return simple_instruction("MOD", offset);
        case OP_MULT: return simple_instruction("MULT", offset);
        case OP_POW: return simple_instruction("POW", offset);

        /* Variable operations */
        case OP_DEFGLOBAL: return constant_instruction("DEFGLOBAL", vm, bytecode, offset);
        case OP_GETGLOBAL: return constant_instruction("GETGLOBAL", vm, bytecode, offset);
        case OP_SETGLOBAL: return constant_instruction("SETGLOBAL", vm, bytecode, offset);
        case OP_GETUPVAL: return byte_instruction("GETUPVAL", bytecode, offset);
        case OP_SETUPVAL: return byte_instruction("SETUPVAL", bytecode, offset);
        case OP_CLOSEUPVAL: return simple_instruction("CLOSEUPVAL", offset);
        case OP_GETLOCAL: return byte_instruction("GETLOCAL", bytecode, offset);
        case OP_SETLOCAL: return byte_instruction("SETLOCAL", bytecode, offset);

        /* Jump/loop operations */
        case OP_JUMP: return jump_instruction("JUMP", 1, bytecode, offset);
        case OP_JUMPIFF: return jump_instruction("JUMPIFF", 1, bytecode, offset);
        case OP_LOOP: return jump_instruction("LOOP", -1, bytecode, offset);

        /* Closures/functions operations */
        case OP_CLOSURE: return closure_instruction("CLOSURE", vm, bytecode, offset);
        case OP_CALL: return byte_instruction("CALL", bytecode, offset);
        case OP_RETURN: return simple_instruction("RETURN", offset);

        /* Class operations */
        case OP_DEFCLASS: return constant_instruction("DEFCLASS", vm, bytecode, offset);
        case OP_INHERIT: return simple_instruction("INHERIT", offset);
        case OP_DEFMETHOD: return constant_instruction("DEFMETHOD", vm, bytecode, offset);
        case OP_INVPROP: return invoke_instruction("INVPROP", vm, bytecode, offset);
        case OP_GETPROP: return constant_instruction("GETPROP", vm, bytecode, offset);
        case OP_SETPROP: return constant_instruction("SETPROP", vm, bytecode, offset);
        case OP_SUPER: return constant_instruction("SUPER", vm, bytecode, offset);
        case OP_INVSUPER: return invoke_instruction("INVSUPER", vm, bytecode, offset);

        /* VM operations */
        case OP_DUPT: return simple_instruction("DUPT", offset);
        case OP_POPT: return simple_instruction("POPT", offset);
        case OP_POPEXPR: return simple_instruction("POPEXPR", offset);
        case OP_TEMP:
            return simple_instruction("TEMP", offset); /* Should not be reachable */

        /* Unknown opcode */
        default: printf("Unknown opcode %d\n", instruction); return offset + 1;
    }
}

/**
 * Prints to stdout a complete bytecode chunk, including its opcodes and constants.
 */
void dump_bytecode(FalconVM *vm, ObjFunction *function) {
    const ObjString *functionName = function->name;
    const BytecodeChunk *bytecode = &function->bytecode;
    bool isTopLevel = (functionName == NULL);

    PRINT_OPCODE_HEADER(isTopLevel, functionName->chars, vm->fileName);
    for (int offset = 0; offset < bytecode->count;)
        offset = dump_instruction(vm, bytecode, offset); /* Disassembles each instruction */

    if (!isTopLevel) printf("\n");
}

/**
 * Traces the execution of a given call frame, printing to stdout the virtual machine stack and the
 * opcodes that operate on the stack.
 */
void trace_execution(FalconVM *vm, CallFrame *frame) {
    if (vm->stack != vm->stackTop) dump_stack(vm);
    dump_instruction(vm, &frame->closure->function->bytecode,
                     (int) (frame->pc - frame->closure->function->bytecode.code));
}

/**
 * Prints to stdout the current state of the virtual machine stack.
 */
void dump_stack(FalconVM *vm) {
    printf("Stack:  ");
    for (FalconValue *slot = vm->stack; slot < vm->stackTop; slot++) {
        printf("[ ");
        print_value(vm, *slot);
        printf(" ] ");
    }
    printf("\n");
}

/**
 * Prints to stdout debug information on the allocation of a FalconObj.
 */
void dump_allocation(FalconObj *object, size_t size, ObjType type) {
    printf("Allocated %ld bytes for type \"%s\" at address %p\n", size, get_object_name(type),
           (void *) object);
}

/**
 * Prints to stdout debug information on the freeing of an allocated FalconObj.
 */
void dump_free(FalconObj *object) {
    printf("Freed object from type \"%s\" at address %p\n", get_object_name(object->type),
           (void *) object);
}

/**
 * Prints to stdout the current status of the garbage collector.
 */
void dump_GC_status(const char *status) { printf("== Garbage Collector %s ==\n", status); }

/**
 * Prints to stdout debug information on the "marking" of a FalconObj for garbage collection.
 */
void dump_mark(FalconObj *object) { printf("Object at address %p marked\n", (void *) object); }

/**
 * Prints to stdout debug information on the "blackening" of a FalconObj for garbage collection.
 */
void dump_blacken(FalconObj *object) {
    printf("Object at address %p blackened\n", (void *) object);
}

/**
 * Prints to stdout the number of collected bytes after a garbage collection process, and the
 * number of bytes required for the next garbage collector activation.
 */
void dump_GC(FalconVM *vm, size_t bytesAllocated) {
    printf("Collected %ld bytes (from %ld to %ld)\n", bytesAllocated - vm->bytesAllocated,
           bytesAllocated, vm->bytesAllocated);
    printf("Next GC at %ld bytes\n", vm->nextGC);
}
