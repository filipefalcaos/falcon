/*
 * YAPL version 0.0.1 (master, Oct 18 2019)
 * yapl_error.c: YAPL's compiler/vm error treatment
 * See YAPL's license in the LICENSE file
 */

#include "yapl_error.h"
#include "../utils/yapl_utils.h"
#include <stdio.h>

/**
 * Presents a compiler time error to the programmer.
 */
void compileTimeError(Token *token, const char *message) {
    int tkLine = token->line;
    int tkColumn = token->column;
    const char *fileName = vm.fileName;
    const char *sourceLine = getSourceFromLine();

    fprintf(stderr, "%s:%d:%d => ", fileName, tkLine, tkColumn); /* Prints file and line */
    fprintf(stderr, "CompilerError: %s\n", message);             /* Prints error message */
    fprintf(stderr, "%d | ", tkLine);
    printUntil(stderr, sourceLine, '\n'); /* Prints source line */
    fprintf(stderr, "\n");
    fprintf(stderr, "%*c^\n", tkColumn + getDigits(tkLine) + 2, ' '); /* Prints error indicator */
}

/**
 * Prints a stack trace of call frames from a given initial one to a given final one.
 */
static void printCallFrames(int initial, int final) {
    for (int i = initial; i >= final; i--) {
        CallFrame *currentFrame = &vm.frames[i];
        ObjFunction *currentFunction = currentFrame->closure->function;
        size_t currentInstruction = currentFrame->pc - currentFunction->bytecodeChunk.code - 1;
        int currentLine = getSourceLine(&currentFrame->closure->function->bytecodeChunk,
                                        (int) currentInstruction);

        /* Prints line and function name */
        fprintf(stderr, "    [Line %d] in ", currentLine);
        if (currentFunction->name == NULL) {
            fprintf(stderr, "%s\n", SCRIPT_TAG);
        } else {
            fprintf(stderr, "%s()\n", currentFunction->name->chars);
        }
    }
}

/**
 * Presents a runtime error to the programmer.
 */
void runtimeError(const char *format, va_list args) {
    fprintf(stderr, "RuntimeError: ");
    vfprintf(stderr, format, args); /* Prints the error */
    fprintf(stderr, "\n");

    /* Prints a stack trace */
    fprintf(stderr, "Stack trace (last call first):\n");
    if (vm.frameCount > 20) {
        printCallFrames(vm.frameCount - 1, vm.frameCount - 10);
        fprintf(stderr, "    ...\n");
        printCallFrames(9, 0);
        fprintf(stderr, "%d call frames not listed. Run with option \"--debug\" to see all.\n",
                vm.frameCount - 20);
    } else {
        printCallFrames(vm.frameCount - 1, 0);
    }
}
