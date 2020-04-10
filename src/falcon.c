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
    if ((input) && *(input)) add_history(input)

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
 * Prints Falcon's interpreter usage details.
 */
void printUsage() {
    printf("usage: %s\n", FALCON_USAGE);
    printf("Available options: \n"
           "  " FALCON_DEBUG_OPT "\n"
           "  " FALCON_HELP_OPT "\n"
           "  " FALCON_INPUT_OPT "\n"
           "  " FALCON_VERSION_OPT "\n"
           "  " FALCON_STOP_OPT "\n"
           "  " FALCON_SCRIPT_ARG "\n");
}

/**
 * Interprets a Falcon source file.
 */
static void runFile(FalconVM *vm) {
    char *source = readFile(vm, vm->fileName);                 /* Gets the source content */
    FalconResultCode resultCode = falconInterpret(vm, source); /* Interprets the source code */
    free(source);
    if (resultCode == FALCON_COMPILE_ERROR) exit(FALCON_ERR_COMPILER);
    if (resultCode == FALCON_RUNTIME_ERROR) exit(FALCON_ERR_RUNTIME);
}

/**
 * Sets the Falcon file interpreter.
 */
void setFile(FalconVM *vm, const char *fileName) {
    vm->isREPL = false;
    vm->fileName = fileName;
    runFile(vm); /* Interprets the input file */
}

/**
 * Sets the Falcon command interpreter.
 */
void setCommand(FalconVM *vm, const char *inputCommand) {
    vm->isREPL = false;
    vm->fileName = FALCON_INPUT;
    falconInterpret(vm, inputCommand); /* Interprets the input command */
}

/**
 * Reads an input line from the terminal by using the "FALCON_READLINE" macro.
 */
char *readLine() {
    char *input;
#ifndef FALCON_READLINE_AVAILABLE
    char inputLine[FALCON_REPL_MAX];
    input = inputLine; /* Fixed array needed for fgets */
    (void) input;      /* Unused */
#endif

    FALCON_READLINE(input, FALCON_PROMPT); /* Reads the input line */
    return input;
}

/**
 * Starts Falcon REPL procedure: read (R) a source line, evaluate (E) it, print (P) the results,
 * and loop (L) back.
 */
static void repl(FalconVM *vm) {
    while (true) {
        char *input = readLine();     /* Reads the input line */
        if (!input) {                 /* Checks if failed to read */
            FALCON_FREE_INPUT(input); /* Frees the input line */
            fprintf(stderr, "%s\n", IO_READLINE_ERR);
            exit(FALCON_ERR_OS);
        }

        FALCON_ADD_HISTORY(input);  /* Adds history to the REPL */
        falconInterpret(vm, input); /* Interprets the source line */
    }
}

/**
 * Sets the Falcon interactive mode (i.e., REPL).
 */
static void setRepl(FalconVM *vm) {
    vm->fileName = FALCON_REPL;
    vm->isREPL = true;
    printInfo();
    printHelp();
    repl(vm); /* Starts the REPL */
}

/* CLI parsing errors */
#define UNKNOWN_OPT_ERR  "Unknown option '%s'.\n"
#define REQUIRED_ARG_ERR "Option '%s' requires a string argument.\n"

/* Reports a given CLI error and then prints the CLI usage */
#define CLI_ERROR(argv, index, error)          \
    do {                                       \
        fprintf(stderr, error, (argv)[index]); \
        printUsage();                          \
        exit(FALCON_ERR_USAGE);                \
    } while (false)

/* Checks if there are no extra characters after the first char */
#define CHECK_EXTRA_CHARS(argv, index) ((argv)[index][2] != '\0')

/* Checks if there are no arguments for the last parsed option */
#define CHECK_NO_ARG(argv, i) ((argv)[i] == NULL || (argv)[i][0] == '-')

/* Validates the current option, reporting an error if there extra chars after the given index */
#define VALIDATE_OPTION(argv, index)                                                 \
    do {                                                                             \
        if (CHECK_EXTRA_CHARS(argv, index)) CLI_ERROR(argv, index, UNKNOWN_OPT_ERR); \
    } while (false)

/* Validates the argument of the current option, reporting an error if there is no argument after
 * the option */
#define VALIDATE_ARG(argv, index)                                                    \
    do {                                                                             \
        if (CHECK_NO_ARG(argv, index)) CLI_ERROR(argv, index - 1, REQUIRED_ARG_ERR); \
    } while (false)

/**
 * Processes the given CLI arguments and proceeds with the requested action. The following options
 * are available:
 *
 * "-d"        output basic interpreter debugging information
 * "-h"        output usage information
 * "-i input"  input code to execute (ends the option list)
 * "-v"        output version information
 * "--"        stop parsing options
 * script      script file to interpret
 *
 * The "-i" option requires a string argument and ends the parsing of the option list. Both the "-h"
 * and "-v" options print their information immediately after being parsed, and then exit the
 * interpreter. The "--" option is used to stop parsing the option list.
 *
 * After parsing the list, if there was no "-i" option, the CLI will look for the script positional
 * argument. If no value is found, the interpreter will start on REPL mode. If there was a "-i"
 * option, the interpreter will execute the command passed as argument to that option.
 */
static void processArgs(FalconVM *vm, int argc, char **argv) {
    char *inputCommand = NULL;
    size_t optionId;

    /* Parses all command line arguments */
    for (optionId = 1; optionId < argc && argv[optionId][0] == '-'; optionId++) {
        switch (argv[optionId][1]) {
            case 'd':
                VALIDATE_OPTION(argv, optionId);
                vm->dumpOpcodes = true;
                break;
            case 'h':
                VALIDATE_OPTION(argv, optionId);
                printUsage();
                exit(FALCON_NO_ERR);
            case 'i':
                VALIDATE_OPTION(argv, optionId);
                optionId++;
                VALIDATE_ARG(argv, optionId);
                inputCommand = argv[optionId];
                goto EXEC; /* Stop parsing on "-i"*/
            case 'v':
                VALIDATE_OPTION(argv, optionId);
                printInfo();
                exit(FALCON_NO_ERR);
            case '-':
                optionId++;
                goto EXEC; /* Stop parsing on "--" */
            default:
                CLI_ERROR(argv, optionId, UNKNOWN_OPT_ERR);
        }
    }

    /* Execute the given source */
    EXEC:
    if (inputCommand != NULL) {
        setCommand(vm, inputCommand); /* Sets the command interpreter */
    } else if (argv[optionId] == NULL) {
        setRepl(vm); /* Sets the Falcon REPL */
    } else {
        setFile(vm, argv[optionId]); /* Sets the file interpreter */
    }
}

#undef UNKNOWN_OPT_ERR
#undef REQUIRED_ARG_ERR
#undef CLI_ERROR
#undef CHECK_EXTRA_CHARS
#undef CHECK_NO_ARG
#undef VALIDATE_OPTION
#undef VALIDATE_ARG

int main(int argc, char **argv) {
    FalconVM vm;
    initFalconVM(&vm);
    processArgs(&vm, argc, argv);
    freeFalconVM(&vm);
    return 0;
}
