/*
 * YAPL version 0.0.1 (master, Oct 18 2019)
 * yapl.c: YAPL stand-alone interpreter (REPL)
 * See YAPL's license in the LICENSE file
 */

#include "yapl.h"
#include "lib/io/yapl_io.h"
#include "vm/yapl_vm.h"
#include <stdlib.h>

/* Defines the "yapl_readline", "yapl_add_history", and "yapl_free_input" macros to handle the
 * user input in REPL mode.
 * If the "readline" lib is available, uses the "readline" and "add_history" functions from the lib
 * to handle user input and history management.
 * Otherwise, uses the standard IO "fputs" and "fgets" functions to handle user input and disables
 * history management */
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
static void runFile(VM *vm) {
    char *source = readFile(vm->fileName);         /* Gets the source content */
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
    input = inputLine; /* Fixed array needed for fgets */
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

/* CLI parsing errors */
#define UNKNOWN_OPT_ERROR  "Unknown option '%s'.\n"
#define REQUIRED_ARG_ERROR "Option '%s' requires a string argument.\n"

/* Reports a given CLI error and then prints the CLI usage */
#define CLI_ERROR(argv, index, error)        \
    do {                                     \
        fprintf(stderr, error, argv[index]); \
        printUsage();                        \
        exit(YAPL_ERR_USAGE);                \
    } while (false)

/* Checks if there are no extra characters in the same option. If so, reports a CLI error through
 * CLI_ERROR */
#define CHECK_EXTRA_CHARS(argv, index)                                     \
    do {                                                                   \
        if (argv[i][2] != '\0') CLI_ERROR(argv, index, UNKNOWN_OPT_ERROR); \
    } while (false)

/* Checks if there are no arguments for the last parsed option */
#define CHECK_NO_ARG(argv, i) (argv[i] == NULL || argv[i][0] == '-')

/**
 * Processes the given CLI arguments and proceeds with the requested action. The following options
 * are available:
 *
 * "-i <input>" input code to execute
 * "-h"         output usage information
 * "-v"         output version information
 * <script>     script file to interpret
 *
 * If there are no CLI arguments, the interpreter will start on REPL mode. If a input "-i" option
 * or a script path is provided, the help "-h" and version "-v" options will be ignored. If both
 * "-h" and "-v" options are provided, only the help option will be executed.
 */
static void processArgs(VM *vm, int argc, char **argv) {
    char *fileName, *inputCommand;
    bool hasHelp, hasVersion, hasScript;
    fileName = inputCommand = NULL;
    hasHelp = hasVersion = hasScript = false;

    if (argc == 1) { /* No arguments provided? */
        repl(vm);    /* Starts the REPL */
        return;
    }

    /* Parses all command line arguments */
    for (int i = 1; argv[i] != NULL; i++) {
        if (argv[i][0] != '-') { /* Not an option? */
            hasScript = true;
            fileName = argv[i];
        } else { /* Is an option */
            switch (argv[i][1]) {
                case 'h':
                    CHECK_EXTRA_CHARS(argv, i);
                    hasHelp = true;
                    break;
                case 'v':
                    CHECK_EXTRA_CHARS(argv, i);
                    hasVersion = true;
                    break;
                case 'i':
                    CHECK_EXTRA_CHARS(argv, i);
                    i++; /* Goes to the next arg */

                    if (CHECK_NO_ARG(argv, i)) { /* Has no argument or is another option? */
                        CLI_ERROR(argv, i - 1, REQUIRED_ARG_ERROR);
                    } else {
                        hasScript = true;
                        inputCommand = argv[i];
                    }

                    break;
                default:
                    CLI_ERROR(argv, i, UNKNOWN_OPT_ERROR);
            }
        }
    }

    if (!hasScript) {
        if (hasVersion && !hasHelp) printInfo(); /* Prints version details */
        if (hasHelp) printUsage();               /* Prints usage */
    } else {
        if (inputCommand != NULL) {
            vm->fileName = REPL;
            vm->isREPL = true;
            interpret(vm, inputCommand); /* Interprets the input command */
        } else {
            vm->fileName = fileName;
            vm->isREPL = false;
            runFile(vm); /* Interprets the input file */
        }
    }
}

#undef CLI_ERROR
#undef CHECK_EXTRA_CHARS
#undef CHECK_NO_ARG

int main(int argc, char **argv) {
    VM vm;
    initVM(&vm);
    processArgs(&vm, argc, argv);
    freeVM(&vm);
    return 0;
}
