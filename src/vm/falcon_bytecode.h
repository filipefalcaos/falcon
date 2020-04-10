/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * falcon_bytecode.h: Falcon's bytecode chunk
 * See Falcon's license in the LICENSE file
 */

#ifndef FALCON_BYTECODE_H
#define FALCON_BYTECODE_H

#include "../commons.h"
#include "falcon_opcodes.h"
#include "falcon_value.h"

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
void initBytecode(BytecodeChunk *bytecode);
void freeBytecode(FalconVM *vm, BytecodeChunk *bytecode);
void writeBytecode(FalconVM *vm, BytecodeChunk *bytecode, uint8_t byte, int line);
int getLine(const BytecodeChunk *bytecode, int instruction);
int addConstant(FalconVM *vm, BytecodeChunk *bytecode, FalconValue value);
void writeConstant(FalconVM *vm, BytecodeChunk *bytecode, uint16_t index, int line);

#endif // FALCON_BYTECODE_H
