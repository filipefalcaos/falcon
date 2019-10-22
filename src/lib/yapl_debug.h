/*
 * YAPL version 0.0.1 (master, Oct 18 2019)
 * yapl_debug.h: List of debugging function for the YAPL virtual machine
 * See YAPL's license in the LICENSE file
 */

#ifndef YAPL_DEBUG_H
#define YAPL_DEBUG_H

#include "../vm/yapl_bytecode_chunk.h"

/* Virtual machine debugging operations */
void disassembleBytecodeChunk(BytecodeChunk *bytecodeChunk, const char *name);
int disassembleInstruction(BytecodeChunk *bytecodeChunk, int offset);
void printOpcodesHeader();
void printTraceExecutionHeader();

#endif // YAPL_DEBUG_H
