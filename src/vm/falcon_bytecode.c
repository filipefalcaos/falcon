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
void FalconInitBytecode(FalconBytecodeChunk *bytecode) {
    bytecode->count = 0;
    bytecode->capacity = 0;
    bytecode->lineCount = 0;
    bytecode->lineCapacity = 0;
    bytecode->code = NULL;
    bytecode->lines = NULL;
    FalconInitValues(&bytecode->constants);
}

/**
 * Frees a bytecode chunk.
 */
void FalconFreeBytecode(FalconVM *vm, FalconBytecodeChunk *bytecode) {
    FALCON_FREE_ARRAY(vm, uint8_t, bytecode->code, bytecode->capacity);
    FALCON_FREE_ARRAY(vm, SourceLine, bytecode->lines, bytecode->lineCapacity);
    FalconFreeValues(vm, &bytecode->constants);
    FalconInitBytecode(bytecode);
}

/**
 * Appends a byte to the end of a bytecode chunk. If the current size is not enough, the capacity
 * of the bytecode chunk is increased to fit the new byte.
 */
void FalconWriteBytecode(FalconVM *vm, FalconBytecodeChunk *bytecode, uint8_t byte, int line) {
    if (bytecode->capacity < bytecode->count + 1) { /* Checks if should increase */
        int oldCapacity = bytecode->capacity;
        bytecode->capacity = FALCON_INCREASE_CAPACITY(oldCapacity); /* Increase the capacity */
        bytecode->code =
            FALCON_INCREASE_ARRAY(vm, bytecode->code, uint8_t, oldCapacity,
                                  bytecode->capacity); /* Increase the bytecode chunk */

        if (bytecode->code == NULL) { /* Checks if the allocation failed */
            FalconMemoryError();
            return;
        }
    }

    bytecode->code[bytecode->count] = byte; /* Set byte */
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

        if (bytecode->lines == NULL) { /* Checks if the allocation failed */
            FalconMemoryError();
            return;
        }
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
int FalconGetLine(FalconBytecodeChunk *bytecode, int instruction) {
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
int FalconAddConstant(FalconVM *vm, FalconBytecodeChunk *bytecode, FalconValue value) {
    FalconPush(vm, value); /* Adds to stack to avoid being garbage collected */
    FalconWriteValues(vm, &bytecode->constants, value);
    FalconPop(vm); /* Removes from the stack */
    return bytecode->constants.count - 1;
}

/**
 * Writes a 2 bytes constant to the bytecode chunk.
 */
void FalconWriteConstant(FalconVM *vm, FalconBytecodeChunk *bytecode, uint16_t index, int line) {
    FalconWriteBytecode(vm, bytecode, FALCON_OP_CONSTANT, line);
    FalconWriteBytecode(vm, bytecode, (uint8_t)(index & 0xffu), line);
    FalconWriteBytecode(vm, bytecode, (uint8_t)((uint16_t)(index >> 8u) & 0xffu), line);
}
