/*
 * YAPL version 0.0.1 (master, Oct 18 2019)
 * yapl_bytecode_chunk.h: YAPL's bytecode chunk
 * See YAPL's license in the LICENSE file
 */

#ifndef YAPL_BYTECODE_CHUNK_H
#define YAPL_BYTECODE_CHUNK_H

#include "../commons.h"
#include "yapl_opcodes.h"
#include "yapl_value.h"

/* Source code line representation */
typedef struct {
    int offset;
    int line;
} SourceLine;

/* Bytecode chunk representation */
typedef struct {
    int count;
    int capacity;
    int lineCount;
    int lineCapacity;
    uint8_t *code;
    SourceLine *lines;
    ValueArray constants;
} BytecodeChunk;

/* Bytecode chunk operations */
void initBytecodeChunk(BytecodeChunk *bytecodeChunk);
void freeBytecodeChunk(BytecodeChunk *bytecodeChunk);
void writeToBytecodeChunk(BytecodeChunk *bytecodeChunk, uint8_t byte, int line);
int getSourceLine(BytecodeChunk *bytecodeChunk, int instruction);
int addConstant(BytecodeChunk *bytecodeChunk, Value value);
void writeConstant(BytecodeChunk *bytecodeChunk, int index, int line);

#endif // YAPL_BYTECODE_CHUNK_H
