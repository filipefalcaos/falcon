/*
 * YAPL version 0.0.1 (master, Oct 18 2019)
 * yapl_scanner.h: YAPL's handwritten scanner
 * See YAPL's license in the LICENSE file
 */

#ifndef YAPL_SCANNER_H
#define YAPL_SCANNER_H

#include "yapl_tokens.h"

/* YAPL's token representation */
typedef struct {
    TokenType type;
    const char *start;
    int length;
    int line;
    int column;
} Token;

/* Scanning operations */
void initScanner(const char *source);
const char *getSourceFromLine();
Token scanToken();

/* Scanning error messages */
#define UNTERMINATED_STR_ERR "Unterminated string."
#define UNEXPECTED_TK_ERR    "Unexpected token."

#endif // YAPL_SCANNER_H
