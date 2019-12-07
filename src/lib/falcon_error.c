/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * falcon_error.c: Falcon's Compiler/VM error treatment
 * See Falcon's license in the LICENSE file
 */

#include "falcon_error.h"
#include "falcon_io.h"
#include "falcon_math.h"

/* Stack trace limits */
#define FALCON_MAX_STACK_TRACE 20

/**
 * Presents a compiler time error to the programmer.
 */
void falconCompileError(FalconVM *vm, Scanner *scanner, Token *token, const char *message) {
    uint32_t tkLine = token->line;
    uint32_t tkColumn = token->column;
    const char *fileName = vm->fileName;
    const char *sourceLine = falconGetSourceFromLine(scanner);

    fprintf(stderr, "%s:%d:%d => ", fileName, tkLine, tkColumn); /* Prints file and line */
    fprintf(stderr, "CompilerError: %s\n", message);             /* Prints error message */
    fprintf(stderr, "%d | ", tkLine);
    falconPrintUntil(stderr, sourceLine, '\n'); /* Prints source line */
    fprintf(stderr, "\n");
    fprintf(stderr, "%*c^\n", tkColumn + falconGetDigits(tkLine) + 2,
            ' '); /* Prints error indicator */
}

/**
 * Prints a stack trace of call frames from a given initial one to a given final one.
 */
static void printCallFrames(FalconVM *vm, int initial, int final) {
    for (int i = initial; i >= final; i--) {
        CallFrame *currentFrame = &vm->frames[i];
        ObjFunction *currentFunction = currentFrame->closure->function;
        size_t currentInstruction = currentFrame->pc - currentFunction->bytecode.code - 1;
        int currentLine =
            falconGetLine(&currentFrame->closure->function->bytecode, (int) currentInstruction);

        /* Prints line and function name */
        fprintf(stderr, "    [Line %d] in ", currentLine);
        if (currentFunction->name == NULL) {
            fprintf(stderr, "%s\n", FALCON_SCRIPT);
        } else {
            fprintf(stderr, "%s()\n", currentFunction->name->chars);
        }
    }
}

/**
 * Presents a runtime error to the programmer.
 */
void falconRuntimeError(FalconVM *vm, const char *format, va_list args) {
    fprintf(stderr, "RuntimeError: ");
    vfprintf(stderr, format, args); /* Prints the error */
    fprintf(stderr, "\n");

    /* Prints a stack trace */
    fprintf(stderr, "Stack trace (last call first):\n");
    if (vm->frameCount > FALCON_MAX_STACK_TRACE) {
        printCallFrames(vm, vm->frameCount - 1, vm->frameCount - (FALCON_MAX_STACK_TRACE / 2));
        fprintf(stderr, "    ...\n");
        printCallFrames(vm, (FALCON_MAX_STACK_TRACE / 2) - 1, 0);
        fprintf(stderr, "%d call frames not listed. Run with option \"--debug\" to see all.\n",
                vm->frameCount - FALCON_MAX_STACK_TRACE);
    } else {
        printCallFrames(vm, vm->frameCount - 1, 0);
    }
}
