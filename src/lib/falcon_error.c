/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * falcon_error.c: Falcon's Compiler/VM error treatment
 * See Falcon's license in the LICENSE file
 */

#include "falcon_error.h"
#include "io/falcon_io.h"
#include "math/falcon_math.h"

/* Stack trace limits */
#define FALCON_MAX_STACK_TRACE 20

/**
 * Presents a compiler time error to the programmer.
 */
void FalconCompileTimeError(FalconVM *vm, FalconScanner *scanner, FalconToken *token,
                            const char *message) {
    int tkLine = token->line;
    int tkColumn = token->column;
    const char *fileName = vm->fileName;
    const char *sourceLine = FalconGetSourceFromLine(scanner);

    fprintf(stderr, "%s:%d:%d => ", fileName, tkLine, tkColumn); /* Prints file and line */
    fprintf(stderr, "CompilerError: %s\n", message);             /* Prints error message */
    fprintf(stderr, "%d | ", tkLine);
    FalconPrintUntil(stderr, sourceLine, '\n'); /* Prints source line */
    fprintf(stderr, "\n");
    fprintf(stderr, "%*c^\n", tkColumn + FalconGetDigits(tkLine) + 2, ' '); /* Prints error indicator */
}

/**
 * Prints a stack trace of call frames from a given initial one to a given final one.
 */
static void FalconPrintCallFrames(FalconVM *vm, int initial, int final) {
    for (int i = initial; i >= final; i--) {
        FalconCallFrame *currentFrame = &vm->frames[i];
        FalconObjFunction *currentFunction = currentFrame->closure->function;
        size_t currentInstruction = currentFrame->pc - currentFunction->bytecodeChunk.code - 1;
        int currentLine = FalconGetLine(&currentFrame->closure->function->bytecodeChunk,
                                        (int) currentInstruction);

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
void FalconRuntimeError(FalconVM *vm, const char *format, va_list args) {
    fprintf(stderr, "RuntimeError: ");
    vfprintf(stderr, format, args); /* Prints the error */
    fprintf(stderr, "\n");

    /* Prints a stack trace */
    fprintf(stderr, "Stack trace (last call first):\n");
    if (vm->frameCount > FALCON_MAX_STACK_TRACE) {
        FalconPrintCallFrames(vm, vm->frameCount - 1, vm->frameCount - (FALCON_MAX_STACK_TRACE / 2));
        fprintf(stderr, "    ...\n");
        FalconPrintCallFrames(vm, (FALCON_MAX_STACK_TRACE / 2) - 1, 0);
        fprintf(stderr, "%d call frames not listed. Run with option \"--debug\" to see all.\n",
                vm->frameCount - FALCON_MAX_STACK_TRACE);
    } else {
        FalconPrintCallFrames(vm, vm->frameCount - 1, 0);
    }
}
