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
    FalconValueArray constants;
} FalconBytecodeChunk;

/* Bytecode chunk operations */
void FalconInitBytecode(FalconBytecodeChunk *bytecodeChunk);
void FalconFreeBytecode(FalconBytecodeChunk *bytecodeChunk);
void FalconWriteBytecode(FalconBytecodeChunk *bytecodeChunk, uint8_t byte, int line);
int FalconGetLine(FalconBytecodeChunk *bytecodeChunk, int instruction);
int FalconAddConstant(FalconBytecodeChunk *bytecodeChunk, FalconValue value);
void FalconWriteConstant(FalconBytecodeChunk *bytecodeChunk, uint16_t index, int line);

#endif // FALCON_BYTECODE_H
