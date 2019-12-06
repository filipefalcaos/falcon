/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * falcon_scanner.c: Falcons's handwritten Scanner
 * See Falcon's license in the LICENSE file
 */

#include "falcon_scanner.h"
#include "../commons.h"
#include <string.h>

/**
 * Initializes the scanner with the first character of the first source code line.
 */
void FalconInitScanner(const char *source, FalconScanner *scanner) {
    scanner->start = source;
    scanner->current = source;
    scanner->lineContent = source;
    scanner->line = 1;
    scanner->column = 0;
}

/**
 * Gets the current line in the scanner.
 */
const char *FalconGetSourceFromLine(FalconScanner *scanner) { return scanner->lineContent; }

/**
 * Check if a character is a valid alpha.
 */
static bool isAlpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }

/**
 * Check if a character is a digit.
 */
static bool isDigit(char c) { return c >= '0' && c <= '9'; }

/**
 * Check if the scanner reached the EOF.
 */
static bool reachedEOF(FalconScanner *scanner) { return *scanner->current == '\0'; }

/**
 * Advance the scanner to read the next character from the input source code.
 */
static char advance(FalconScanner *scanner) {
    scanner->current++;
    scanner->column++;
    return scanner->current[-1];
}

/**
 * Gets the current character in the scanner.
 */
static char peek(FalconScanner *scanner) { return *scanner->current; }

/**
 * Gets the next character in the scanner, without advancing.
 */
static char peekNext(FalconScanner *scanner) {
    if (reachedEOF(scanner)) return '\0';
    return scanner->current[1];
}

/**
 * Checks if a character matches the current one on the scanner. If so, advances to the next
 * character in the scanner.
 */
static bool match(char expected, FalconScanner *scanner) {
    if (reachedEOF(scanner) || peek(scanner) != expected) {
        return false;
    } else {
        scanner->current++;
        scanner->column++;
        return true;
    }
}

/**
 * Makes a token of a certain token type using the current name in the scanner.
 */
static FalconToken makeToken(FalconTokenType type, FalconScanner *scanner) {
    FalconToken token;
    token.type = type;
    token.start = scanner->start;
    token.length = (uint32_t)(scanner->current - scanner->start);
    token.line = scanner->line;
    token.column = scanner->column;
    return token;
}

/**
 * Makes the error token (FALCON_TK_ERROR) with a message to present to the programmer.
 */
static FalconToken errorToken(const char *message, FalconScanner *scanner) {
    FalconToken token;
    token.type = FALCON_TK_ERROR;
    token.start = message;
    token.length = (uint32_t) strlen(message);
    token.line = scanner->line;
    token.column = (uint32_t) scanner->start;
    return token;
}

/**
 * Handles unnecessary characters in the input source code. When a whitespace, a tab, a new line,
 * a carriage return, or a comment is the current character, the scanner advances to the next one.
 */
static void preProcessSource(FalconScanner *scanner) {
    while (true) {
        char c = peek(scanner);
        switch (c) {
            case ' ':
            case '\r':
            case '\t':
                advance(scanner);
                break;
            case '\n':
                scanner->line++;
                advance(scanner);
                scanner->column = 0;
                scanner->lineContent = scanner->current;
                break;
            case '#':
                while (peek(scanner) != '\n' && !reachedEOF(scanner))
                    advance(scanner); /* Loop on comments */
                break;
            default:
                return;
        }
    }
}

/**
 * Checks if a name matches a keyword. To match, the lexeme must be exactly as long as the keyword
 * and the remaining characters must match exactly.
 */
static FalconTokenType checkKeyword(int start, int length, const char *rest, FalconTokenType type,
                                    FalconScanner *scanner) {
    if (scanner->current - scanner->start == start + length &&
        memcmp(scanner->start + start, rest, (size_t) length) == 0)
        return type;
    return FALCON_TK_IDENTIFIER;
}

/**
 * Checks if a name is an identifier or a defined Falcon keyword.
 */
static FalconTokenType findTokenType(FalconScanner *scanner) {
    const char start = scanner->start[0]; /* Current char */

    switch (start) {
        case 'a':
            return checkKeyword(1, 2, "nd", FALCON_TK_AND, scanner);
        case 'b':
            return checkKeyword(1, 4, "reak", FALCON_TK_BREAK, scanner);
        case 'c':
            return checkKeyword(1, 4, "lass", FALCON_TK_CLASS, scanner);
        case 'e':
            return checkKeyword(1, 3, "lse", FALCON_TK_ELSE, scanner);
        case 'f':
            if (scanner->current - scanner->start > 1) {
                switch (scanner->start[1]) {
                    case 'a':
                        return checkKeyword(2, 3, "lse", FALCON_TK_FALSE, scanner);
                    case 'n':
                        return checkKeyword(2, 0, "", FALCON_TK_FUNCTION, scanner);
                    case 'o':
                        return checkKeyword(2, 1, "r", FALCON_TK_FOR, scanner);
                    default:
                        break;
                }
            }
            break;
        case 'i':
            return checkKeyword(1, 1, "f", FALCON_TK_IF, scanner);
        case 'n':
            if (scanner->current - scanner->start > 1) {
                switch (scanner->start[1]) {
                    case 'e':
                        return checkKeyword(2, 2, "xt", FALCON_TK_NEXT, scanner);
                    case 'o':
                        return checkKeyword(2, 1, "t", FALCON_TK_NOT, scanner);
                    case 'u':
                        return checkKeyword(2, 2, "ll", FALCON_TK_NULL, scanner);
                    default:
                        break;
                }
            }
            break;
        case 'o':
            return checkKeyword(1, 1, "r", FALCON_TK_OR, scanner);
        case 'r':
            return checkKeyword(1, 5, "eturn", FALCON_TK_RETURN, scanner);
        case 's':
            if (scanner->current - scanner->start > 1) {
                switch (scanner->start[1]) {
                    case 'u':
                        return checkKeyword(2, 3, "per", FALCON_TK_SUPER, scanner);
                    case 'w':
                        return checkKeyword(2, 4, "itch", FALCON_TK_SWITCH, scanner);
                    default:
                        break;
                }
            }
            break;
        case 't':
            if (scanner->current - scanner->start > 1) {
                switch (scanner->start[1]) {
                    case 'h':
                        return checkKeyword(2, 2, "is", FALCON_TK_THIS, scanner);
                    case 'r':
                        return checkKeyword(2, 2, "ue", FALCON_TK_TRUE, scanner);
                    default:
                        break;
                }
            }
            break;
        case 'v':
            return checkKeyword(1, 2, "ar", FALCON_TK_VAR, scanner);
        case 'w': {
            const char *current = scanner->start;
            long length = scanner->current - scanner->start; /* Current token length */

            if (length > 1) {
                switch (current[1]) {
                    case 'h': {
                        if (length > 2) {
                            switch (current[2]) {
                                case 'e':
                                    return checkKeyword(3, 1, "n", FALCON_TK_WHEN, scanner);
                                case 'i':
                                    return checkKeyword(3, 2, "le", FALCON_TK_WHILE, scanner);
                                default:
                                    break;
                            }
                        }
                    }
                    default:
                        break;
                }
            }

            break;
        }
        default:
            return FALCON_TK_IDENTIFIER;
    }

    return FALCON_TK_IDENTIFIER;
}

/**
 * Extracts an identifier from the source code. A valid identifier contains alphas and digits (only
 * after the first alpha).
 */
static FalconToken identifier(FalconScanner *scanner) {
    while (isAlpha(peek(scanner)) || isDigit(peek(scanner))) advance(scanner);
    return makeToken(findTokenType(scanner), scanner);
}

/**
 * Extracts a numeric value from the source code.
 */
static FalconToken number(FalconScanner *scanner) {
    while (isDigit(peek(scanner))) advance(scanner);

    /* Fractional number */
    if (peek(scanner) == '.' && isDigit(peekNext(scanner))) {
        advance(scanner);
        while (isDigit(peek(scanner))) advance(scanner);
    }

    return makeToken(FALCON_TK_NUMBER, scanner);
}

/**
 * Extracts a string value from the source code.
 */
static FalconToken string(FalconScanner *scanner) {
    while (peek(scanner) != '"' && !reachedEOF(scanner)) {
        if (peek(scanner) == '\n') {
            scanner->line++;
            scanner->column = 0;
            scanner->lineContent = scanner->current;
        }

        advance(scanner);
    }

    if (reachedEOF(scanner)) /* Checks if is an unterminated string */
        return errorToken(FALCON_UNTERMINATED_STR_ERR, scanner);

    advance(scanner);
    return makeToken(FALCON_TK_STRING, scanner);
}

/**
 * Main scanner function. Scans and returns the next token in the source code.
 */
FalconToken FalconScanToken(FalconScanner *scanner) {
    preProcessSource(scanner);         /* Handles unnecessary characters */
    scanner->start = scanner->current; /* Sets the start point to the last token */
    if (reachedEOF(scanner)) return makeToken(FALCON_TK_EOF, scanner);

    char nextChar = advance(scanner);                  /* Gets the next character */
    if (isAlpha(nextChar)) return identifier(scanner); /* Checks if is an alpha */
    if (isDigit(nextChar)) return number(scanner);     /* Checks if is an digit */

    switch (nextChar) { /* Checks for lexemes matching */
        case '(':
            return makeToken(FALCON_TK_LEFT_PAREN, scanner);
        case ')':
            return makeToken(FALCON_TK_RIGHT_PAREN, scanner);
        case '{':
            return makeToken(FALCON_TK_LEFT_BRACE, scanner);
        case '}':
            return makeToken(FALCON_TK_RIGHT_BRACE, scanner);
        case '?':
            return makeToken(FALCON_TK_TERNARY, scanner);
        case ':':
            return makeToken(FALCON_TK_COLON, scanner);
        case ';':
            return makeToken(FALCON_TK_SEMICOLON, scanner);
        case ',':
            return makeToken(FALCON_TK_COMMA, scanner);
        case '.':
            return makeToken(FALCON_TK_DOT, scanner);
        case '-':
            if (match('=', scanner))
                return makeToken(FALCON_TK_MINUS_EQUAL, scanner);
            else if (match('>', scanner))
                return makeToken(FALCON_TK_ARROW, scanner);
            else
                return makeToken(FALCON_TK_MINUS, scanner);
        case '+':
            return makeToken(match('=', scanner) ? FALCON_TK_PLUS_EQUAL : FALCON_TK_PLUS, scanner);
        case '/':
            return makeToken(match('=', scanner) ? FALCON_TK_DIV_EQUAL : FALCON_TK_DIV, scanner);
        case '%':
            return makeToken(match('=', scanner) ? FALCON_TK_MOD_EQUAL : FALCON_TK_MOD, scanner);
        case '*':
            return makeToken(match('=', scanner) ? FALCON_TK_MULTIPLY_EQUAL : FALCON_TK_MULTIPLY,
                             scanner);
        case '^':
            return makeToken(match('=', scanner) ? FALCON_TK_POW_EQUAL : FALCON_TK_POW, scanner);
        case '!':
            if (match('=', scanner)) /* Logical not operator is "not" instead of "!" */
                return makeToken(FALCON_TK_NOT_EQUAL, scanner);
        case '=':
            return makeToken(match('=', scanner) ? FALCON_TK_EQUAL_EQUAL : FALCON_TK_EQUAL,
                             scanner);
        case '<':
            return makeToken(match('=', scanner) ? FALCON_TK_LESS_EQUAL : FALCON_TK_LESS, scanner);
        case '>':
            return makeToken(match('=', scanner) ? FALCON_TK_GREATER_EQUAL : FALCON_TK_GREATER,
                             scanner);
        case '"':
            return string(scanner);
        default:
            return errorToken(FALCON_UNEXPECTED_TK_ERR, scanner);
    }
}
