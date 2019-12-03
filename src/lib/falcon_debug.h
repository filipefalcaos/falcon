/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * falcon_debug.h: List of debugging function for the Falcon Compiler/VM
 * See Falcon's license in the LICENSE file
 */

#ifndef FALCON_DEBUG_H
#define FALCON_DEBUG_H

#include "../vm/falcon_bytecode.h"

/* Virtual machine debugging operations */
void FalconOpcodesHeader();
void FalconExecutionHeader();
int FalconDisassembleInst(FalconBytecodeChunk *bytecodeChunk, int offset);
void FalconDisassembleBytecode(FalconBytecodeChunk *bytecodeChunk, const char *name);

#endif // FALCON_DEBUG_H
