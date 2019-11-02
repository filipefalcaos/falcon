/*
 * YAPL version 0.0.1 (master, Oct 18 2019)
 * yapl.h: YAPL - Yet Another Programming Language
 * See YAPL's license in the LICENSE file
 */

#ifndef YAPL_H
#define YAPL_H

/* YAPL functions */
void printAuthors();
void printLicense();
void printUsage();

/* YAPL version and copyright */
#define YAPL_VERSION_MAJOR   "0"
#define YAPL_VERSION_MINOR   "0"
#define YAPL_VERSION_RELEASE "2"
#define YAPL_VERSION_TYPE    "master"
#define YAPL_VERSION_DATE    "Aug 15 2019"
#define YAPL_VERSION         "YAPL version " YAPL_VERSION_MAJOR "." YAPL_VERSION_MINOR
#define YAPL_RELEASE         YAPL_VERSION "." YAPL_VERSION_RELEASE
#define YAPL_COPYRIGHT       YAPL_RELEASE ", Copyright (c) 2019 Filipe Falcão"
#define YAPL_AUTHORS         "Filipe Falcão, UFAL, Brazil"

/* REPL macros */
#define PROMPT         ">>> "
#define SCRIPT_TAG     "<script>"
#define YAPL_USAGE     "Usage: yapl [options] [file]"
#define HELP_OPTION    "-h, --help   output usage information"
#define VERSION_OPTION "-v, --version   output version information"
#define YAPL_OPTIONS   HELP_OPTION "\n" VERSION_OPTION
#define FILE_ARG       "file   script file to interpret"
#define YAPL_ARGS      FILE_ARG
#define MORE_INFO      "See full license in the \"LICENSE\" file"
#define HELP_FUNC      "\"help()\""
#define LICENSE_FUNC   "\"license()\""
#define AUTHORS_FUNC   "\"authors()\""
#define YAPL_HELP                                                        \
    "Call " HELP_FUNC ", " LICENSE_FUNC " or " AUTHORS_FUNC " for more " \
    "information."

/* Limits */
/* TODO: make the last two work as configurable parameters */
#define MAX_SINGLE_BYTE (UINT8_MAX + 1) /* 256 */
#define VM_FRAMES_MAX   1000
#define VM_STACK_MAX    (VM_FRAMES_MAX * MAX_SINGLE_BYTE) /* 256000 */

/* Error codes */
#define YAPL_ERR_USAGE    1
#define YAPL_ERR_COMPILER 2
#define YAPL_ERR_RUNTIME  3

#endif // YAPL_H
