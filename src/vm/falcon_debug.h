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
int falconDumpInstruction(FalconVM *vm, BytecodeChunk *bytecode, int offset);
void falconDumpBytecode(FalconVM *vm, BytecodeChunk *bytecode, const char *name);
void falconDumpStack(FalconVM *vm);

/* Memory allocation debugging operations */
void falconDumpAllocation(FalconObj *object, size_t size, ObjType type);
void falconDumpFree(FalconObj *object);

/* Garbage collection debugging operations */
void falconGCStatus(const char *status);
void falconDumpMark(FalconObj *object);
void falconDumpBlacken(FalconObj *object);
void falconDumpGC(FalconVM *vm, size_t bytesAllocated);

#endif // FALCON_DEBUG_H
