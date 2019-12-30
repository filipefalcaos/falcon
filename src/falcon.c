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
 * Prints Falcon's authors.
 */
void falconPrintAuthors() { printf("Falcon authors: %s\n", FALCON_AUTHORS); }

/**
 * Prints Falcon's MIT license.
 */
void falconPrintLicense() { printf("%s\n%s\n", FALCON_COPYRIGHT, FALCON_MORE_INFO); }

/**
 * Prints Falcon's interpreter usage details.
 */
void falconPrintUsage() {
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
            fprintf(stderr, "%s\n", FALCON_READLINE_ERR);
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
#define UNKNOWN_OPT_ERROR  "Unknown option '%s'.\n"
#define REQUIRED_ARG_ERROR "Option '%s' requires a string argument.\n"

/* Reports a given CLI error and then prints the CLI usage */
#define CLI_ERROR(argv, index, error)          \
    do {                                       \
        fprintf(stderr, error, (argv)[index]); \
        falconPrintUsage();                    \
        exit(FALCON_ERR_USAGE);                \
    } while (false)

/* Checks if there are no extra characters in the same option. If so, reports a CLI error through
 * CLI_ERROR */
#define CHECK_EXTRA_CHARS(argv, index)                                       \
    do {                                                                     \
        if ((argv)[i][2] != '\0') CLI_ERROR(argv, index, UNKNOWN_OPT_ERROR); \
    } while (false)

/* Checks if there are no arguments for the last parsed option */
#define CHECK_NO_ARG(argv, i) ((argv)[i] == NULL || (argv)[i][0] == '-')

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
        setRepl(vm); /* Sets the Falcon REPL */
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
        if (hasHelp) falconPrintUsage();         /* Prints usage */
    } else {
        if (inputCommand != NULL) {
            setCommand(vm, inputCommand); /* Sets the command interpreter */
        } else {
            setFile(vm, fileName); /* Sets the file interpreter */
        }
    }
}

#undef CLI_ERROR
#undef CHECK_EXTRA_CHARS
#undef CHECK_NO_ARG

int main(int argc, char **argv) {
    FalconVM vm;
    falconInitVM(&vm);
    processArgs(&vm, argc, argv);
    falconFreeVM(&vm);
    return 0;
}
