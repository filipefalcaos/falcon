/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * fl-bcode.c: Falcon's bytecode chunk
 * See Falcon's license in the LICENSE file
 */

#include "fl-bcode.h"
#include "fl-mem.h"

/**
 * Initializes an empty bytecode chunk.
 */
void init_bytecode(BytecodeChunk *bytecode) {
    bytecode->count = 0;
    bytecode->capacity = 0;
    bytecode->lineCount = 0;
    bytecode->lineCapacity = 0;
    bytecode->code = NULL;
    bytecode->lines = NULL;
    init_value_array(&bytecode->constants);
}

/**
 * Frees a previously allocated bytecode chunk and its list of constants.
 */
void free_bytecode(FalconVM *vm, BytecodeChunk *bytecode) {
    FALCON_FREE_ARRAY(vm, uint8_t, bytecode->code, bytecode->capacity);
    FALCON_FREE_ARRAY(vm, SourceLine, bytecode->lines, bytecode->lineCapacity);
    free_value_array(vm, &bytecode->constants);
    init_bytecode(bytecode);
}

/**
 * Appends a byte to the end of a bytecode chunk. If the current size is not enough, the capacity
 * of the bytecode chunk is increased to fit the new byte.
 */
void write_bytecode(FalconVM *vm, BytecodeChunk *bytecode, uint8_t byte, int line) {
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
int get_source_line(const BytecodeChunk *bytecode, int instruction) {
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
 * Adds a new constant to the constants list of a bytecode chunk and returns its index.
 */
int add_constant(FalconVM *vm, BytecodeChunk *bytecode, FalconValue value) {
    DISABLE_GC(vm); /* Avoids GC from the "write_value_array" ahead */
    write_value_array(vm, &bytecode->constants, value); /* Adds the constant */
    ENABLE_GC(vm);
    return bytecode->constants.count - 1;
}
