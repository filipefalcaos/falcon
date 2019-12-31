/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * falcon_error.c: Falcon's Compiler/VM error treatment
 * See Falcon's license in the LICENSE file
 */

#include "falcon_error.h"
#include "falcon_io.h"
#include <math.h>

/**
 * Presents a compiler time error to the programmer.
 */
static void falconCompileError(FalconVM *vm, Scanner *scanner, Token *token, const char *message) {
    int offset = 0;
    uint32_t tkLine = token->line;
    uint32_t tkColumn = token->column;
    const char *fileName = vm->fileName;
    const char *sourceLine = getSourceFromLine(scanner);

    /* Prints the file, the line, the error message, and the source line */
    fprintf(stderr, "%s:%d:%d => ", fileName, tkLine, tkColumn);
    fprintf(stderr, "CompilerError: %s\n", message);
    fprintf(stderr, "%d | ", tkLine);
    printUntil(stderr, sourceLine, '\n');

    /* Prints error indicator */
    if (token->type == TK_EOF) offset = 1;
    fprintf(stderr, "\n");
    fprintf(stderr, "%*c^\n", tkColumn + (int) floor(log10(tkLine) + 1) + 2 + offset, ' ');
}

/**
 * Presents a syntax/compiler error to the programmer.
 */
void falconCompilerError(FalconCompiler *compiler, Token *token, const char *message) {
    if (compiler->parser->panicMode) return; /* Checks and sets error recovery */
    compiler->parser->panicMode = true;
    falconCompileError(compiler->vm, compiler->scanner, token, message); /* Presents the error */
    compiler->parser->hadError = true;
}

/**
 * Prints a stack trace of call frames from a given initial one to a given final one.
 */
static void printCallFrames(FalconVM *vm, int initial, int final) {
    for (int i = initial; i >= final; i--) {
        CallFrame *currentFrame = &vm->frames[i];
        ObjFunction *currentFunction = currentFrame->closure->function;
        size_t currentInstruction = currentFrame->pc - currentFunction->bytecode.code - 1;
        int currentLine = getLine(&currentFunction->bytecode, (int) currentInstruction);

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
    if (vm->frameCount > FALCON_MAX_TRACE) {
        printCallFrames(vm, vm->frameCount - 1, vm->frameCount - (FALCON_MAX_TRACE / 2));
        fprintf(stderr, "    ...\n");
        printCallFrames(vm, (FALCON_MAX_TRACE / 2) - 1, 0);
        fprintf(stderr, "%d call frames not listed. Run with option \"--debug\" to see all.\n",
                vm->frameCount - FALCON_MAX_TRACE);
    } else {
        printCallFrames(vm, vm->frameCount - 1, 0);
    }
}

/**
 * Presents a runtime error to the programmer and resets the VM stack.
 */
void falconVMError(FalconVM *vm, const char *format, ...) {
    va_list args;
    va_start(args, format);
    falconRuntimeError(vm, format, args); /* Presents the error */
    va_end(args);
    resetVMStack(vm); /* Resets the stack due to error */
}
