/*
 * YAPL version 0.0.1 (master, Oct 18 2019)
 * yapl_bytecode_chunk.c: YAPL's bytecode chunk
 * See YAPL's license in the LICENSE file
 */

#include "yapl_bytecode_chunk.h"
#include "../yapl.h"
#include "yapl_memory_manager.h"

/**
 * Initializes an empty bytecode chunk.
 */
void initBytecodeChunk(BytecodeChunk *bytecodeChunk) {
    bytecodeChunk->count = 0;
    bytecodeChunk->capacity = 0;
    bytecodeChunk->lineCount = 0;
    bytecodeChunk->lineCapacity = 0;
    bytecodeChunk->code = NULL;
    bytecodeChunk->lines = NULL;
    initValueArray(&bytecodeChunk->constants);
}

/**
 * Frees a bytecode chunk.
 */
void freeBytecodeChunk(BytecodeChunk *bytecodeChunk) {
    FREE_ARRAY(uint8_t, bytecodeChunk->code, bytecodeChunk->capacity);
    FREE_ARRAY(SourceLine, bytecodeChunk->lines, bytecodeChunk->lineCapacity);
    freeValueArray(&bytecodeChunk->constants);
    initBytecodeChunk(bytecodeChunk);
}

/**
 * Appends a byte to the end of a bytecode chunk. If the current size is not enough, the capacity
 * of the bytecode chunk is increased to fit the new byte.
 */
void writeToBytecodeChunk(BytecodeChunk *bytecodeChunk, uint8_t byte, int line) {
    if (bytecodeChunk->capacity < bytecodeChunk->count + 1) { /* Checks if should increase */
        int oldCapacity = bytecodeChunk->capacity;
        bytecodeChunk->capacity = INCREASE_CAPACITY(oldCapacity); /* Increase the capacity */
        bytecodeChunk->code =
            INCREASE_ARRAY(bytecodeChunk->code, uint8_t, oldCapacity,
                           bytecodeChunk->capacity); /* Increase the bytecode chunk */
    }

    bytecodeChunk->code[bytecodeChunk->count] = byte; /* Set byte */
    bytecodeChunk->count++;

    /* Checks if still the same line */
    if (bytecodeChunk->lineCount > 0 &&
        bytecodeChunk->lines[bytecodeChunk->lineCount - 1].line == line) {
        return;
    }

    if (bytecodeChunk->lineCapacity < bytecodeChunk->lineCount + 1) { /* Checks if new line */
        int oldCapacity = bytecodeChunk->lineCapacity;
        bytecodeChunk->lineCapacity = INCREASE_CAPACITY(oldCapacity); /* Increases the capacity */
        bytecodeChunk->lines =
            INCREASE_ARRAY(bytecodeChunk->lines, SourceLine, oldCapacity,
                           bytecodeChunk->lineCapacity); /* Increases the lines list */
    }

    SourceLine *sourceLine = &bytecodeChunk->lines[bytecodeChunk->lineCount++]; /* Sets the line */
    sourceLine->offset = bytecodeChunk->count - 1;
    sourceLine->line = line;
}

/**
 * Searches for the line that contains a given instruction, through a binary search. This procedure
 * is only possible because YAPL is single-pass compiled, which means instruction codes can only
 * increase. Thus, the array is always sorted and a binary search is possible.
 */
int getSourceLine(BytecodeChunk *bytecodeChunk, int instruction) {
    int start = 0;
    int end = bytecodeChunk->lineCount - 1;

    while (true) {
        int middle = (start + end) / 2;
        SourceLine *sourceLine = &bytecodeChunk->lines[middle];

        if (instruction < sourceLine->offset) {
            end = middle - 1;
        } else if (middle == bytecodeChunk->lineCount - 1 ||
                   instruction < bytecodeChunk->lines[middle + 1].offset) {
            return sourceLine->line;
        } else {
            start = middle + 1;
        }
    }
}

/**
 * Adds a new constant to a bytecode chunk.
 */
int addConstant(BytecodeChunk *bytecodeChunk, Value value) {
    writeValueArray(&bytecodeChunk->constants, value);
    return bytecodeChunk->constants.count - 1;
}

/**
 * Writes a constant to the bytecode chunk.
 */
int writeConstant(BytecodeChunk *bytecodeChunk, Value value, int line) {
    int index = addConstant(bytecodeChunk, value);

    if (index < MAX_SINGLE_BYTE) {
        writeToBytecodeChunk(bytecodeChunk, OP_CONSTANT, line);
        writeToBytecodeChunk(bytecodeChunk, (uint8_t) index, line);
    } else {
        writeToBytecodeChunk(bytecodeChunk, OP_CONSTANT_16, line);
        writeToBytecodeChunk(bytecodeChunk, (uint8_t) (index & 0xff), line);
        writeToBytecodeChunk(bytecodeChunk, (uint8_t) ((index >> 8) & 0xff), line);
    }

    return index;
}
