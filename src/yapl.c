/*
 * YAPL version 0.0.1 (master, Oct 18 2019)
 * yapl.c: YAPL stand-alone interpreter (REPL)
 * See YAPL's license in the LICENSE file
 */

#include "yapl.h"
#include "lib/io/yapl_io.h"
#include "vm/yapl_vm.h"
#include <readline/readline.h>
#include <stdlib.h>
#include <string.h>

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
    printf("%s\n\nOptions and arguments:\n%s\n%s\n", YAPL_USAGE, YAPL_OPTIONS, YAPL_ARGS);
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
    char *inputLine;
    printInfo();
    printHelp();

    while (true) {
        inputLine = readline(PROMPT); /* Reads the input line */
        add_history(inputLine);       /* Adds history to the REPL */

        if (!inputLine) { /* Checks if failed to read */
            free(inputLine);
            break;
        }

        interpret(vm, inputLine); /* Interprets the source line */
    }

    free(inputLine); /* Frees the input line when over */
}

/**
 * Process the CLI args and proceeds with the requested action.
 */
static void processArgs(VM *vm, int argc, char const **argv) {
    if (argc == 1) {
        vm->fileName = "repl";
        repl(vm); /* Starts the REPL */
    } else if (argc == 2) {
        if (argv[1][0] == '-' ||
            (argv[1][0] == '-' && argv[1][1] == '-')) { /* Checks if arg is an option */
            if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
                printUsage(); /* Prints usage */
            } else if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0) {
                printInfo(); /* Prints version details */
            } else {         /* Unknown option: prints the usage */
                printf("Unknown option '%s'\n", argv[1]);
                printUsage();
                exit(YAPL_ERR_USAGE);
            }
        } else {
            vm->fileName = argv[1];
            runFile(vm, argv[1]); /* Interprets the input file */
        }
    } else { /* Unknown option: prints the usage */
        printf("Wrong arguments order\n");
        printUsage();
        exit(YAPL_ERR_USAGE);
    }
}

int main(int argc, char const **argv) {
    VM vm;
    initVM(&vm);
    processArgs(&vm, argc, argv);
    freeVM(&vm);
    return 0;
}
