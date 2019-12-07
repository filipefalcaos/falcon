/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * falcon.c: The Falcon Programming Language
 * See Falcon's license in the LICENSE file
 */

#include "falcon.h"
#include "lib/falcon_io.h"
#include "vm/falcon_vm.h"
#include <stdlib.h>

/* Defines the "FALCON_READLINE", "FALCON_ADD_HISTORY", and "FALCON_FREE_INPUT" macros to handle
 * the user input in REPL mode.
 * If the "readline" lib is available, uses the "readline" and "add_history" functions from the lib
 * to handle user input and history management.
 * Otherwise, uses the standard IO "fputs" and "fgets" functions to handle user input and disables
 * history management */
#ifdef FALCON_READLINE_AVAILABLE

#include <string.h>

#if __APPLE__ || __linux__ || __unix__ /* MacOS only requires "readline/readline.h" */
#include <readline/readline.h>
#endif
#if __linux__ || __unix__ /* Linux/Unix requires both */
#include <readline/history.h>
#endif

/* Use "readline" lib for input and history */
#define FALCON_READLINE(input, prompt) input = readline(prompt)
#define FALCON_FREE_INPUT(input)       free(input)
#define FALCON_ADD_HISTORY(input) \
    if (strlen(input) > 0) add_history(input)

#else

/* Use fgets for input and disables history */
#define FALCON_READLINE(input, prompt) \
    (fputs(prompt, stdout), fflush(stdout), fgets(input, FALCON_REPL_MAX, stdin))
#define FALCON_FREE_INPUT(input)  ((void) input)
#define FALCON_ADD_HISTORY(input) ((void) input)

#endif

/**
 * Prints help functions for the REPL.
 */
static void printHelp() { printf("%s\n", FALCON_HELP); }

/**
 * Prints details and usage of the Falcon's REPL.
 */
static void printInfo() {
    printf("%s (%s, %s)\n", FALCON_RELEASE, FALCON_VERSION_TYPE, FALCON_VERSION_DATE);
#ifdef CMAKE_C_COMPILER_ID /* C compiler name from CMake */
    printf("[%s %s] on %s\n", CMAKE_C_COMPILER_ID, CMAKE_C_COMPILER_VERSION, CMAKE_SYSTEM_NAME);
#endif
}

/**
 * Prints Falcon's authors.
 */
void FalconPrintAuthors() { printf("Falcon authors: %s\n", FALCON_AUTHORS); }

/**
 * Prints Falcon's MIT license.
 */
void FalconPrintLicense() { printf("%s\n%s\n", FALCON_COPYRIGHT, FALCON_MORE_INFO); }

/**
 * Prints Falcon's interpreter usage details.
 */
void FalconPrintUsage() {
    printf("Usage: \n%*c%s\n\n", 4, ' ', FALCON_USAGE);
    printf("Flags: \n%*c%s\n%*c%s\n\n", 4, ' ', FALCON_HELP_FLAG, 4, ' ', FALCON_VERSION_FLAG);
    printf("Options: \n%*c%s\n\n", 4, ' ', FALCON_INPUT_OPTION);
    printf("Arguments: \n%*c%s\n", 4, ' ', FALCON_FILE_ARG);
}

/**
 * Interprets a Falcon source file.
 */
static void runFile(FalconVM *vm) {
    char *source = falconReadFile(vm, vm->fileName);           /* Gets the source content */
    FalconResultCode resultCode = falconInterpret(vm, source); /* Interprets the source code */
    free(source);
    if (resultCode == FALCON_COMPILE_ERROR) exit(FALCON_ERR_COMPILER);
    if (resultCode == FALCON_RUNTIME_ERROR) exit(FALCON_ERR_RUNTIME);
}

/**
 * Starts Falcon REPL.
 */
static void repl(FalconVM *vm) {
    char *input;
#ifndef FALCON_READLINE_AVAILABLE
    char inputLine[FALCON_REPL_MAX];
    input = inputLine; /* Fixed array needed for fgets */
    (void) input;      /* Unused */
#endif

    printInfo();
    printHelp();

    while (true) {
        FALCON_READLINE(input, FALCON_PROMPT); /* Reads the input line */
        if (!input) {                          /* Checks if failed to read */
            free(input);
            break;
        }

        FALCON_ADD_HISTORY(input);  /* Adds history to the REPL */
        falconInterpret(vm, input); /* Interprets the source line */
    }

    FALCON_FREE_INPUT(input); /* Frees the input line when over */
}

/* CLI parsing errors */
#define FALCON_UNKNOWN_OPT_ERROR "Unknown option '%s'.\n"
#define FALCON_REQUIRED_ARG_ERROR "Option '%s' requires a string argument.\n"

/* Reports a given CLI error and then prints the CLI usage */
#define FALCON_CLI_ERROR(argv, index, error)   \
    do {                                       \
        fprintf(stderr, error, (argv)[index]); \
        FalconPrintUsage();                    \
        exit(FALCON_ERR_USAGE);                \
    } while (false)

/* Checks if there are no extra characters in the same option. If so, reports a CLI error through
 * FALCON_CLI_ERROR */
#define FALCON_CHECK_EXTRA_CHARS(argv, index)                                              \
    do {                                                                                   \
        if ((argv)[i][2] != '\0') FALCON_CLI_ERROR(argv, index, FALCON_UNKNOWN_OPT_ERROR); \
    } while (false)

/* Checks if there are no arguments for the last parsed option */
#define FALCON_CHECK_NO_ARG(argv, i) ((argv)[i] == NULL || (argv)[i][0] == '-')

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
static void processArgs(FalconVM *vm, int argc, char **argv) {
    char *fileName, *inputCommand;
    bool hasHelp, hasVersion, hasScript;
    fileName = inputCommand = NULL;
    hasHelp = hasVersion = hasScript = false;

    if (argc == 1) { /* No arguments provided? */
        vm->fileName = FALCON_REPL;
        vm->isREPL = true;
        repl(vm); /* Starts the REPL */
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
                    FALCON_CHECK_EXTRA_CHARS(argv, i);
                    hasHelp = true;
                    break;
                case 'v':
                    FALCON_CHECK_EXTRA_CHARS(argv, i);
                    hasVersion = true;
                    break;
                case 'i':
                    FALCON_CHECK_EXTRA_CHARS(argv, i);
                    i++; /* Goes to the next arg */

                    if (FALCON_CHECK_NO_ARG(argv, i)) { /* Has no argument or is another option? */
                        FALCON_CLI_ERROR(argv, i - 1, FALCON_REQUIRED_ARG_ERROR);
                    } else {
                        hasScript = true;
                        inputCommand = argv[i];
                    }

                    break;
                default:
                    FALCON_CLI_ERROR(argv, i, FALCON_UNKNOWN_OPT_ERROR);
            }
        }
    }

    if (!hasScript) {
        if (hasVersion && !hasHelp) printInfo(); /* Prints version details */
        if (hasHelp) FalconPrintUsage();         /* Prints usage */
    } else {
        if (inputCommand != NULL) {
            vm->fileName = FALCON_REPL;
            vm->isREPL = true;
            falconInterpret(vm, inputCommand); /* Interprets the input command */
        } else {
            vm->fileName = fileName;
            vm->isREPL = false;
            runFile(vm); /* Interprets the input file */
        }
    }
}

#undef FALCON_CLI_ERROR
#undef FALCON_CHECK_EXTRA_CHARS
#undef FALCON_CHECK_NO_ARG

int main(int argc, char **argv) {
    FalconVM vm;
    falconInitVM(&vm);
    processArgs(&vm, argc, argv);
    falconFreeVM(&vm);
    return 0;
}
