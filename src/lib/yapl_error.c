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

    fprintf(stderr, "%s:%d:%d => ", fileName, tkLine, tkColumn); /* Prints the file and line */
    fprintf(stderr, "CompilerError: %s\n", message);             /* Prints the error message */
    fprintf(stderr, "%d | %s\n", tkLine, sourceLine);
    fprintf(stderr, "%*c^\n", tkColumn + getDigits(tkLine) + 2, ' ');
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
    for (int i = vm.frameCount - 1; i >= 0; i--) {
        CallFrame *currentFrame = &vm.frames[i];
        ObjFunction *currentFunction = currentFrame->closure->function;
        size_t currentInstruction = currentFrame->pc - currentFunction->bytecodeChunk.code - 1;
        int currentLine = getSourceLine(&currentFrame->closure->function->bytecodeChunk,
                                        (int) currentInstruction);

        fprintf(stderr, "    [Line %d] in ", currentLine);
        if (currentFunction->name == NULL) {
            fprintf(stderr, "%s\n", SCRIPT_TAG);
        } else {
            fprintf(stderr, "%s()\n", currentFunction->name->chars);
        }
    }
}
