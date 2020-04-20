/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * falcon.h: The Falcon Programming Language
 * See Falcon's license in the LICENSE file
 */

#ifndef FALCON_H
#define FALCON_H

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
#define FALCON_PROMPT     ">>> "
#define FALCON_SCRIPT     "script"
#define FALCON_REPL       "repl"
#define FALCON_INPUT      "input"
#define FALCON_USAGE      "falcon [options] ... [-i input | script]"
#define FALCON_MORE_INFO  "See full license in the \"LICENSE\" file"
#define FALCON_LIC_FN     "\"license()\""
#define FALCON_AUTHORS_FN "\"authors()\""
#define FALCON_HELP       "Call " FALCON_LIC_FN " or " FALCON_AUTHORS_FN " for more information."

/* Values and limits */
#define FALCON_REPL_MAX   500
#define FALCON_MAX_BYTE   (UINT8_MAX + 1) /* 256 */
#define FALCON_FRAMES_MAX 1000
#define FALCON_STACK_MAX  (FALCON_FRAMES_MAX * FALCON_MAX_BYTE) /* 256000 */
#define FALCON_MAX_TRACE  20

/* Error codes */
#define FALCON_NO_ERR       0
#define FALCON_ERR_USAGE    1
#define FALCON_ERR_COMPILER 2
#define FALCON_ERR_RUNTIME  3
#define FALCON_ERR_MEMORY   4
#define FALCON_ERR_OS       5

/* Forward declaration of the Falcon Virtual Machine */
typedef struct FalconVM FalconVM;

#endif // FALCON_H
