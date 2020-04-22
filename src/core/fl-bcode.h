/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * fl-bcode.h: Falcon's bytecode chunk
 * See Falcon's license in the LICENSE file
 */

#ifndef FL_BCODE_H
#define FL_BCODE_H

#include "fl-opcodes.h"
#include "fl-value.h"
#include <stdint.h>

/* Marks the beginning of a new source code line and the corresponding offset of the first
 * instruction on that same line */
typedef struct {
    int offset;
    int line;
} SourceLine;

/* A chunk of bytecode. It stores a dynamic array of instructions and a dynamic array of source
 * lines of code. It also stores the list of constants in the bytecode chunk */
typedef struct {
    int count;
    int capacity;
    int lineCount;
    int lineCapacity;
    uint8_t *code;
    SourceLine *lines;
    ValueArray constants;
} BytecodeChunk;

/* Initializes an empty bytecode chunk */
void init_bytecode(BytecodeChunk *bytecode);

/* Frees a previously allocated bytecode chunk and its list of constants */
void free_bytecode(FalconVM *vm, BytecodeChunk *bytecode);

/* Appends a byte to the end of a bytecode chunk */
void write_bytecode(FalconVM *vm, BytecodeChunk *bytecode, uint8_t byte, int line);

/* Searches for the line that contains a given instruction */
int get_source_line(const BytecodeChunk *bytecode, int instruction);

/* Adds a new constant to the constants list of a bytecode chunk and returns its index */
int add_constant(FalconVM *vm, BytecodeChunk *bytecode, FalconValue value);

#endif // FL_BCODE_H
