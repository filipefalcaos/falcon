/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * falcon_bytecode.c: Falcon's bytecode chunk
 * See Falcon's license in the LICENSE file
 */

#include "falcon_bytecode.h"
#include "falcon_memory.h"

/**
 * Initializes an empty bytecode chunk.
 */
void initBytecode(BytecodeChunk *bytecode) {
    bytecode->count = 0;
    bytecode->capacity = 0;
    bytecode->lineCount = 0;
    bytecode->lineCapacity = 0;
    bytecode->code = NULL;
    bytecode->lines = NULL;
    initValArray(&bytecode->constants);
}

/**
 * Frees a bytecode chunk.
 */
void freeBytecode(FalconVM *vm, BytecodeChunk *bytecode) {
    FALCON_FREE_ARRAY(vm, uint8_t, bytecode->code, bytecode->capacity);
    FALCON_FREE_ARRAY(vm, SourceLine, bytecode->lines, bytecode->lineCapacity);
    freeValArray(vm, &bytecode->constants);
    initBytecode(bytecode);
}

/**
 * Appends a byte to the end of a bytecode chunk. If the current size is not enough, the capacity
 * of the bytecode chunk is increased to fit the new byte.
 */
void writeBytecode(FalconVM *vm, BytecodeChunk *bytecode, uint8_t byte, int line) {
    if (bytecode->capacity < bytecode->count + 1) { /* Checks if should increase */
        int oldCapacity = bytecode->capacity;
        bytecode->capacity = FALCON_INCREASE_CAPACITY(oldCapacity); /* Increases the capacity */
        bytecode->code =
            FALCON_INCREASE_ARRAY(vm, bytecode->code, uint8_t, oldCapacity,
                                  bytecode->capacity); /* Increases the bytecode chunk */
    }

    bytecode->code[bytecode->count] = byte; /* Sets the byte */
    bytecode->count++;

    /* Checks if still the same line */
    if (bytecode->lineCount > 0 && bytecode->lines[bytecode->lineCount - 1].line == line) {
        return;
    }

    if (bytecode->lineCapacity < bytecode->lineCount + 1) { /* Checks if new line */
        int oldCapacity = bytecode->lineCapacity;
        bytecode->lineCapacity = FALCON_INCREASE_CAPACITY(oldCapacity); /* Increases the capacity */
        bytecode->lines =
            FALCON_INCREASE_ARRAY(vm, bytecode->lines, SourceLine, oldCapacity,
                                  bytecode->lineCapacity); /* Increases the lines list */
    }

    SourceLine *sourceLine = &bytecode->lines[bytecode->lineCount++]; /* Sets the line */
    sourceLine->offset = bytecode->count - 1;
    sourceLine->line = line;
}

/**
 * Searches for the line that contains a given instruction, through a binary search. This procedure
 * is only possible because Falcon is single-pass compiled, which means instruction codes can only
 * increase. Thus, the array is always sorted and a binary search is possible.
 */
int getLine(BytecodeChunk *bytecode, int instruction) {
    int start = 0;
    int end = bytecode->lineCount - 1;

    while (true) {
        int middle = (start + end) / 2;
        SourceLine *sourceLine = &bytecode->lines[middle];

        if (instruction < sourceLine->offset) {
            end = middle - 1;
        } else if (middle == bytecode->lineCount - 1 ||
                   instruction < bytecode->lines[middle + 1].offset) {
            return sourceLine->line;
        } else {
            start = middle + 1;
        }
    }
}

/**
 * Adds a new constant to a bytecode chunk.
 */
int addConstant(FalconVM *vm, BytecodeChunk *bytecode, FalconValue value) {
    VMPush(vm, value);                              /* Avoids GC */
    writeValArray(vm, &bytecode->constants, value); /* Adds the constant */
    VMPop(vm);
    return bytecode->constants.count - 1;
}

/**
 * Writes a 2 bytes constant to the bytecode chunk.
 */
void writeConstant(FalconVM *vm, BytecodeChunk *bytecode, uint16_t index, int line) {
    writeBytecode(vm, bytecode, OP_LOADCONST, line);
    writeBytecode(vm, bytecode, (uint8_t)(index & 0xffu), line);
    writeBytecode(vm, bytecode, (uint8_t)((uint16_t)(index >> 8u) & 0xffu), line);
}
