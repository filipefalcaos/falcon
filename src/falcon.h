/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * falcon.h: The Falcon Programming Language
 * See Falcon's license in the LICENSE file
 */

#ifndef FALCON_H
#define FALCON_H

/* Falcon functions */
void FalconPrintAuthors();
void FalconPrintLicense();
void FalconPrintUsage();

/* Falcon version and copyright */
#define FALCON_VERSION_MAJOR   "0"
#define FALCON_VERSION_MINOR   "0"
#define FALCON_VERSION_RELEASE "2"
#define FALCON_VERSION_TYPE    "master"
#define FALCON_VERSION_DATE    "Dec 02 2019"
#define FALCON_VERSION         "Falcon version " FALCON_VERSION_MAJOR "." FALCON_VERSION_MINOR
#define FALCON_RELEASE         FALCON_VERSION "." FALCON_VERSION_RELEASE
#define FALCON_COPYRIGHT       FALCON_RELEASE ", Copyright (c) 2019 Filipe Falcão"
#define FALCON_AUTHORS         "Filipe Falcão, UFAL, Brazil"

/* REPL macros */
#define FALCON_PROMPT       ">>> "
#define FALCON_SCRIPT       "<script>"
#define FALCON_REPL         "repl"
#define FALCON_USAGE        "falcon [flags] [options] [script]"
#define FALCON_HELP_FLAG    "-h    output usage information"
#define FALCON_VERSION_FLAG "-v    output version information"
#define FALCON_INPUT_OPTION "-i <input>    input code to execute"
#define FALCON_FILE_ARG     FALCON_SCRIPT "    script file to interpret"
#define FALCON_MORE_INFO    "See full license in the \"LICENSE\" file"
#define FALCON_HELP_FUNC    "\"help()\""
#define FALCON_LIC_FUNC     "\"license()\""
#define FALCON_AUTHORS_FUNC "\"authors()\""
#define FALCON_HELP                                                          \
    "Call " FALCON_HELP_FUNC ", " FALCON_LIC_FUNC " or " FALCON_AUTHORS_FUNC \
    " for more information."

/* Limits */
#define FALCON_REPL_MAX   500
#define FALCON_MAX_BYTE   (UINT8_MAX + 1) /* 256 */
#define FALCON_FRAMES_MAX 1000
#define FALCON_STACK_MAX  (FALCON_FRAMES_MAX * FALCON_MAX_BYTE) /* 256000 */

/* Error codes */
#define FALCON_ERR_USAGE    1
#define FALCON_ERR_COMPILER 2
#define FALCON_ERR_RUNTIME  3
#define FALCON_ERR_MEMORY   4
#define FALCON_ERR_OS       5

#endif // FALCON_H
