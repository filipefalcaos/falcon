/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * falcon_debug.h: List of debugging function for the Falcon Compiler/VM
 * See Falcon's license in the LICENSE file
 */

#ifndef FALCON_DEBUG_H
#define FALCON_DEBUG_H

#include "falcon_bytecode.h"
#include "falcon_object.h"
#include "falcon_vm.h"

/* Debug headers */
#define PRINT_TRACE_HEADER() printf("== Execution Trace ==\n")
#define PRINT_OPCODE_HEADER(isTopLevel, functionName, fileName) \
    printf("== function \"%s\" from <%s> ==\n", isTopLevel ? FALCON_SCRIPT : functionName, fileName)

/* Compiler/VM debugging functions */
int dumpInstruction(FalconVM *vm, const BytecodeChunk *bytecode, int offset);
void dumpBytecode(FalconVM *vm, ObjFunction *function);
void dumpStack(FalconVM *vm);
void traceExecution(FalconVM *vm, CallFrame *frame);

/* Memory allocation debugging functions */
void dumpAllocation(FalconObj *object, size_t size, ObjType type);
void dumpFree(FalconObj *object);

/* Garbage collection debugging functions */
void dumpGCStatus(const char *status);
void dumpMark(FalconObj *object);
void dumpBlacken(FalconObj *object);
void dumpGC(FalconVM *vm, size_t bytesAllocated);

#endif // FALCON_DEBUG_H
