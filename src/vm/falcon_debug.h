/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * falcon_debug.h: List of debugging function for the Falcon Compiler/VM
 * See Falcon's license in the LICENSE file
 */

#ifndef FALCON_DEBUG_H
#define FALCON_DEBUG_H

#include "falcon_bytecode.h"
#include "falcon_object.h"

/* Compiler/VM debugging operations */
void FalconOpcodesHeader();
void FalconExecutionHeader();
int FalconDumpInstruction(FalconBytecodeChunk *bytecode, int offset);
void FalconDumpBytecode(FalconBytecodeChunk *bytecode, const char *name);
void FalconDumpStack(FalconVM *vm);

/* Memory allocation debugging operations */
void FalconDumpAllocation(FalconObj *object, size_t size, FalconObjType type);
void FalconDumpFree(FalconObj *object);

/* Garbage collection debugging operations */
void FalconGCStatus(const char *status);
void FalconDumpMark(FalconObj *object);
void FalconDumpBlacken(FalconObj *object);
void FalconDumpGC(FalconVM *vm, size_t bytesAllocated);

#endif // FALCON_DEBUG_H
