/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * fl-scanner.h: Falcons's handwritten Scanner
 * See Falcon's license in the LICENSE file
 */

#ifndef FL_SCANNER_H
#define FL_SCANNER_H

#include "fl-tokens.h"
#include "fl-value.h"
#include <stdint.h>

/* Token representation */
typedef struct {
    FalconTokens type; /* The token type */
    const char *start; /* The start of the token (pointer to the source) */
    FalconValue value; /* The token value, if it is a literal */
    uint32_t length;   /* The token length (number of chars) */
    uint32_t line;     /* The token line */
    uint32_t column;   /* The token column */
} Token;

/* Scanner representation (lexical analysis) */
typedef struct {
    const char *start;   /* The start of the current token in the scanner */
    const char *current; /* The current token in the scanner */
    const char *source;  /* The input source code */
    uint32_t line;       /* The current source line */
    uint32_t column;     /* The current source column */
} Scanner;

/* Scanning operations */
void init_scanner(const char *source, Scanner *scanner);
const char *get_current_line(Scanner *scanner);
Token scan_token(Scanner *scanner, FalconVM *vm);

/* Scanning error messages */
#define SCAN_BIG_NUM_ERR          "Number literal is too large for an IEEE double."
#define SCAN_UNTERMINATED_STR_ERR "Unterminated string."
#define SCAN_INVALID_ESCAPE       "Invalid escape character."
#define SCAN_UNEXPECTED_TK_ERR    "Unexpected token."

#endif // FL_SCANNER_H
