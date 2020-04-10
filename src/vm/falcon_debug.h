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
int dumpInstruction(FalconVM *vm, const BytecodeChunk *bytecode, int offset);
void dumpBytecode(FalconVM *vm, ObjFunction *function);
void dumpStack(FalconVM *vm);

/* Memory allocation debugging operations */
void dumpAllocation(FalconObj *object, size_t size, ObjType type);
void dumpFree(FalconObj *object);

/* Garbage collection debugging operations */
void dumpGCStatus(const char *status);
void dumpMark(FalconObj *object);
void dumpBlacken(FalconObj *object);
void dumpGC(FalconVM *vm, size_t bytesAllocated);

#endif // FALCON_DEBUG_H
