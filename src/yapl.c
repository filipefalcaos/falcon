/*
 * YAPL version 0.0.1 (master, Oct 18 2019)
 * yapl.c: YAPL stand-alone interpreter (REPL)
 * See YAPL's license in the LICENSE file
 */

#include "yapl.h"
#include "lib/io/yapl_io.h"
#include "vm/yapl_vm.h"
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>

/* Defines the "yapl_readline", "yapl_add_history", and "yapl_free_input" functions to handle the
 * user input in REPL mode.
 * If the "readline" lib is available, uses the "readline" and "add_history" functions from the lib
 * to handle user input and history management.
 * Otherwise, uses the standard "fputs" and "fgets" to handle user input and disables history
 * management */
#ifdef YAPL_READLINE_AVAILABLE

/* Use "readline" lib for input and history */
#include <readline/readline.h>
#include <readline/history.h>
#include <string.h>

#define yapl_readline(input, prompt) input = readline(prompt)
#define yapl_free_input(input)       free(input)
#define yapl_add_history(input) \
    if (strlen(input) > 0) add_history(input)

#else

/* Use fgets for input and disables history */
#define yapl_readline(input, prompt) \
    (fputs(prompt, stdout), fflush(stdout), fgets(input, REPL_MAX_INPUT, stdin))
#define yapl_free_input(input)  ((void) input)
#define yapl_add_history(input) ((void) input)

#endif

/**
 * Prints help functions for the REPL.
 */
static void printHelp() { printf("%s\n", YAPL_HELP); }

/**
 * Prints details and usage of the YAPL's REPL.
 */
static void printInfo() {
    printf("%s (%s, %s)\n", YAPL_RELEASE, YAPL_VERSION_TYPE, YAPL_VERSION_DATE);
#ifdef CMAKE_C_COMPILER_ID /* C compiler name from CMake */
    printf("[%s %s] on %s\n", CMAKE_C_COMPILER_ID, CMAKE_C_COMPILER_VERSION, CMAKE_SYSTEM_NAME);
#endif
}

/**
 * Prints YAPL's authors.
 */
void printAuthors() { printf("YAPL authors: %s\n", YAPL_AUTHORS); }

/**
 * Prints YAPL's MIT license.
 */
void printLicense() { printf("%s\n%s\n", YAPL_COPYRIGHT, MORE_INFO); }

/**
 * Prints YAPL's interpreter usage details.
 */
void printUsage() {
    printf("Usage: \n%*c%s\n\n", 4, ' ', YAPL_USAGE);
    printf("Flags: \n%*c%s\n%*c%s\n\n", 4, ' ', HELP_FLAG, 4, ' ', VERSION_FLAG);
    printf("Options: \n%*c%s\n\n", 4, ' ', INPUT_OPTION);
    printf("Arguments: \n%*c%s\n", 4, ' ', FILE_ARG);
}

/**
 * Interprets a YAPL source file.
 */
static void runFile(VM *vm, const char *path) {
    char *source = readFile(path);                 /* Gets the source content */
    ResultCode resultCode = interpret(vm, source); /* Interprets the source code */
    free(source);
    if (resultCode == COMPILE_ERROR) exit(YAPL_ERR_COMPILER);
    if (resultCode == RUNTIME_ERROR) exit(YAPL_ERR_RUNTIME);
}

/**
 * Starts YAPL REPL.
 */
static void repl(VM *vm) {
    char *input;
#ifndef YAPL_READLINE_AVAILABLE
    char inputLine[REPL_MAX_INPUT];
    input = inputLine;
#endif

    printInfo();
    printHelp();

    while (true) {
        yapl_readline(input, PROMPT); /* Reads the input line */
        if (!input) {                 /* Checks if failed to read */
            free(input);
            break;
        }

        yapl_add_history(input); /* Adds history to the REPL */
        interpret(vm, input);    /* Interprets the source line */
    }

    yapl_free_input(input); /* Frees the input line when over */
}

/**
 * Process the CLI args and proceeds with the requested action.
 */
static void processArgs(VM *vm, int argc, char **argv) {
    int arg;
    char *inputCommand = NULL;
    bool onlyInfo = false, onlyHelp = false;
    opterr = 0; /* Disables getopt automatic error report */

    /* Parses option arguments */
    while ((arg = getopt(argc, argv, "hvi:")) != -1) {
        switch (arg) {
            case 'h':
                onlyHelp = true;
                if (!onlyInfo) printUsage(); /* Prints usage */
                break;
            case 'v':
                onlyInfo = true;
                if (!onlyHelp) printInfo(); /* Prints version details */
                break;
            case 'i':
                inputCommand = optarg; /* Sets "-i" argument */
                break;
            case '?':
                if (!onlyInfo && !onlyHelp) {
                    if (optopt == 'i') {
                        fprintf(stderr, "Option -%c requires an argument.\n", optopt);
                    } else if (isprint(optopt)) {
                        fprintf(stderr, "Unknown option '-%c'.\n", optopt);
                    } else {
                        fprintf(stderr, "Unknown option character '\\x%x'.\n", optopt);
                    }

                    printUsage(); /* Prints usage */
                    exit(YAPL_ERR_USAGE);
                }
            default:
                exit(YAPL_ERR_USAGE);
        }
    }

    /* Parses the [script] positional argument */
    if (!onlyInfo && !onlyHelp) {
        if (argv[optind]) {
            vm->fileName = argv[optind];
            vm->isREPL = false;
            runFile(vm, argv[1]); /* Interprets the input file */
        } else {
            vm->fileName = "repl";
            vm->isREPL = true;
            if (inputCommand != NULL) {
                interpret(vm, inputCommand); /* Interprets the command */
            } else {
                repl(vm); /* Starts the REPL */
            }
        }
    }
}

int main(int argc, char **argv) {
    VM vm;
    initVM(&vm);
    processArgs(&vm, argc, argv);
    freeVM(&vm);
    return 0;
}
