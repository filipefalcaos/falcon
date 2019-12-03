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
void FalconInitBytecode(FalconBytecodeChunk *bytecodeChunk) {
    bytecodeChunk->count = 0;
    bytecodeChunk->capacity = 0;
    bytecodeChunk->lineCount = 0;
    bytecodeChunk->lineCapacity = 0;
    bytecodeChunk->code = NULL;
    bytecodeChunk->lines = NULL;
    FalconInitValues(&bytecodeChunk->constants);
}

/**
 * Frees a bytecode chunk.
 */
void FalconFreeBytecode(FalconBytecodeChunk *bytecodeChunk) {
    FALCON_FREE_ARRAY(uint8_t, bytecodeChunk->code, bytecodeChunk->capacity);
    FALCON_FREE_ARRAY(SourceLine, bytecodeChunk->lines, bytecodeChunk->lineCapacity);
    FalconFreeValues(&bytecodeChunk->constants);
    FalconInitBytecode(bytecodeChunk);
}

/**
 * Appends a byte to the end of a bytecode chunk. If the current size is not enough, the capacity
 * of the bytecode chunk is increased to fit the new byte.
 */
void FalconWriteBytecode(FalconBytecodeChunk *bytecodeChunk, uint8_t byte, int line) {
    if (bytecodeChunk->capacity < bytecodeChunk->count + 1) { /* Checks if should increase */
        int oldCapacity = bytecodeChunk->capacity;
        bytecodeChunk->capacity = FALCON_INCREASE_CAPACITY(oldCapacity); /* Increase the capacity */
        bytecodeChunk->code =
            FALCON_INCREASE_ARRAY(bytecodeChunk->code, uint8_t, oldCapacity,
                           bytecodeChunk->capacity); /* Increase the bytecode chunk */

        if (bytecodeChunk->code == NULL) { /* Checks if the allocation failed */
            FalconMemoryError();
            return;
        }
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
        bytecodeChunk->lineCapacity =
            FALCON_INCREASE_CAPACITY(oldCapacity); /* Increases the capacity */
        bytecodeChunk->lines =
            FALCON_INCREASE_ARRAY(bytecodeChunk->lines, SourceLine, oldCapacity,
                                  bytecodeChunk->lineCapacity); /* Increases the lines list */

        if (bytecodeChunk->lines == NULL) { /* Checks if the allocation failed */
            FalconMemoryError();
            return;
        }
    }

    SourceLine *sourceLine = &bytecodeChunk->lines[bytecodeChunk->lineCount++]; /* Sets the line */
    sourceLine->offset = bytecodeChunk->count - 1;
    sourceLine->line = line;
}

/**
 * Searches for the line that contains a given instruction, through a binary search. This procedure
 * is only possible because Falcon is single-pass compiled, which means instruction codes can only
 * increase. Thus, the array is always sorted and a binary search is possible.
 */
int FalconGetLine(FalconBytecodeChunk *bytecodeChunk, int instruction) {
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
int FalconAddConstant(FalconBytecodeChunk *bytecodeChunk, FalconValue value) {
    FalconWriteValues(&bytecodeChunk->constants, value);
    return bytecodeChunk->constants.count - 1;
}

/**
 * Writes a 2 bytes constant to the bytecode chunk.
 */
void FalconWriteConstant(FalconBytecodeChunk *bytecodeChunk, int index, int line) {
    FalconWriteBytecode(bytecodeChunk, FALCON_OP_CONSTANT, line);
    FalconWriteBytecode(bytecodeChunk, (uint8_t)(index & 0xff), line);
    FalconWriteBytecode(bytecodeChunk, (uint8_t)((index >> 8) & 0xff), line);
}
