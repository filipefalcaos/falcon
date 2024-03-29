/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * fl-debug.h: List of debugging function for the Falcon Compiler/VM
 * See Falcon's license in the LICENSE file
 */

#ifndef FL_DEBUG_H
#define FL_DEBUG_H

#include "fl-bcode.h"
#include "fl-object.h"
#include "fl-vm.h"

/* Debug headers for the "-t" and "-d" command line options */
#define PRINT_TRACE_HEADER() printf("== Execution Trace ==\n")
#define PRINT_OPCODE_HEADER(isTopLevel, functionName, fileName) \
    printf("== function \"%s\" from <%s> ==\n", isTopLevel ? FALCON_SCRIPT : functionName, fileName)

/* Prints to stdout a single instruction from a given bytecode chunk */
int dump_instruction(FalconVM *vm, const BytecodeChunk *bytecode, int offset);

/* Prints to stdout a complete bytecode chunk, including its opcodes and constants */
void dump_bytecode(FalconVM *vm, ObjFunction *function);

/* Prints to stdout the current state of the virtual machine stack */
void dump_stack(FalconVM *vm);

/* Traces the execution of a given call frame, printing to stdout the virtual machine stack and the
 * opcodes that operate on the stack */
void trace_execution(FalconVM *vm, CallFrame *frame);

/* Prints to stdout debug information on the allocation of a FalconObj */
void dump_allocation(FalconObj *object, size_t size, ObjType type);

/* Prints to stdout debug information on the freeing of an allocated FalconObj */
void dump_free(FalconObj *object);

/* Prints to stdout the current status of the garbage collector */
void dump_GC_status(const char *status);

/* Prints to stdout debug information on the "marking" of a FalconObj for garbage collection */
void dump_mark(FalconObj *object);

/* Prints to stdout debug information on the "blackening" of a FalconObj for garbage collection */
void dump_blacken(FalconObj *object);

/* Prints to stdout the number of collected bytes after a garbage collection process, and the
 * number of bytes required for the next garbage collector activation */
void dump_GC(FalconVM *vm, size_t bytesAllocated);

#endif // FL_DEBUG_H
