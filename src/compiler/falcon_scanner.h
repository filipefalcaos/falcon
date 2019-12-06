/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * falcon_scanner.h: Falcons's handwritten Scanner
 * See Falcon's license in the LICENSE file
 */

#ifndef FALCON_SCANNER_H
#define FALCON_SCANNER_H

#include "../commons.h"
#include "falcon_tokens.h"

/* Token representation */
typedef struct {
    FalconTokenType type;
    const char *start;
    uint32_t length;
    uint32_t line;
    uint32_t column;
} FalconToken;

/* Scanner representation (lexical analysis) */
typedef struct {
    const char *start;
    const char *current;
    const char *lineContent;
    uint32_t line;
    uint32_t column;
} FalconScanner;

/* Scanning operations */
void FalconInitScanner(const char *source, FalconScanner *scanner);
const char *FalconGetSourceFromLine(FalconScanner *scanner);
FalconToken FalconScanToken(FalconScanner *scanner);

/* Scanning error messages */
#define FALCON_UNTERMINATED_STR_ERR "Unterminated string."
#define FALCON_UNEXPECTED_TK_ERR "Unexpected token."

#endif // FALCON_SCANNER_H
