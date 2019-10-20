/*
 * YAPL version 0.0.1 (master, Oct 18 2019)
 * yapl_scanner.c: YAPL's handwritten scanner
 * See YAPL's license in the LICENSE file
 */

#include "../include/yapl_scanner.h"
#include "../include/commons.h"
#include <string.h>

/* YAPL's scanner representation (lexical analysis) */
typedef struct {
    const char *start;
    const char *current;
    int line;
    int column;
} Scanner;

/* Scanner instance */
Scanner scanner;

/**
 * Initializes the scanner with the first character of the first source code line.
 */
void initScanner(const char *source) {
    scanner.start = source;
    scanner.current = source;
    scanner.line = 1;
}

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
static bool reachedEOF() { return *scanner.current == '\0'; }

/**
 * Advance the scanner to read the next character from the input source code.
 */
static char advance() {
    scanner.current++;
    scanner.column++;
    return scanner.current[-1];
}

/**
 * Gets the current character in the scanner.
 */
static char peek() { return *scanner.current; }

/**
 * Gets the next character in the scanner, without advancing.
 */
static char peekNext() {
    if (reachedEOF()) return '\0';
    return scanner.current[1];
}

/**
 * Checks if a character matches the current one on the scanner. If so, advances to the next
 * character in the scanner.
 */
static bool match(char expected) {
    if (reachedEOF() || peek() != expected) {
        return false;
    } else {
        scanner.current++;
        scanner.column++;
        return true;
    }
}

/**
 * Makes a token of a certain token type using the current name in the scanner.
 */
static Token makeToken(TokenType type) {
    Token token;
    token.type = type;
    token.start = scanner.start;
    token.length = (int) (scanner.current - scanner.start);
    token.line = scanner.line;
    token.column = scanner.column;
    return token;
}

/**
 * Makes the error token (TK_ERROR) with a message to present to the programmer.
 */
static Token errorToken(const char *message) {
    Token token;
    token.type = TK_ERROR;
    token.start = message;
    token.length = (int) strlen(message);
    token.line = scanner.line;
    token.column = (int) scanner.start;
    return token;
}

/**
 * Handles unnecessary characters in the input source code. When a whitespace, a tab, a new line,
 * a carriage return, or a comment is the current character, the scanner advances to the next one.
 */
static void preProcessSource() {
    for (;;) {
        char c = peek();
        switch (c) {
            case ' ':
            case '\r':
            case '\t':
                advance();
                break;
            case '\n':
                scanner.line++;
                advance();
                break;
            case '#':
                while (peek() != '\n' && !reachedEOF()) /* Loop on comments */
                    advance();
            default:
                return;
        }
    }
}

/**
 * Checks if a name matches a keyword. To match, the lexeme must be exactly as long as the keyword
 * and the remaining characters must match exactly.
 */
static TokenType checkKeyword(int start, int length, const char *rest, TokenType type) {
    if (scanner.current - scanner.start == start + length &&
        memcmp(scanner.start + start, rest, (size_t) length) == 0)
        return type;
    return TK_IDENTIFIER;
}

/**
 * Checks if a name is an identifier or a defined YAPL keyword.
 */
static TokenType findType() {
    switch (scanner.start[0]) {
        case 'a':
            return checkKeyword(1, 2, "nd", TK_AND);
        case 'c':
            return checkKeyword(1, 4, "lass", TK_CLASS);
        case 'e':
            return checkKeyword(1, 3, "lse", TK_ELSE);
        case 'f':
            if (scanner.current - scanner.start > 1) {
                switch (scanner.start[1]) {
                    case 'a':
                        return checkKeyword(2, 3, "lse", TK_FALSE);
                    case 'o':
                        return checkKeyword(2, 1, "r", TK_FOR);
                    case 'u':
                        return checkKeyword(2, 6, "nction", TK_FUNCTION);
                }
            }
            break;
        case 'i':
            return checkKeyword(1, 1, "f", TK_IF);
        case 'n':
            return checkKeyword(1, 3, "ull", TK_NULL);
        case 'o':
            return checkKeyword(1, 1, "r", TK_OR);
        case 'p':
            if (scanner.current - scanner.start > 1) {
                switch (scanner.start[1]) {
                    case 'r':
                        return checkKeyword(2, 3, "int", TK_PRINT);
                    case 'u':
                        return checkKeyword(2, 2, "ts", TK_PUTS);
                }
            }
            break;
        case 'r':
            return checkKeyword(1, 5, "eturn", TK_RETURN);
        case 's':
            return checkKeyword(1, 4, "uper", TK_SUPER);
        case 't':
            if (scanner.current - scanner.start > 1) {
                switch (scanner.start[1]) {
                    case 'h':
                        return checkKeyword(2, 2, "is", TK_THIS);
                    case 'r':
                        return checkKeyword(2, 2, "ue", TK_TRUE);
                }
            }
            break;
        case 'u':
            return checkKeyword(1, 5, "nless", TK_UNLESS);
        case 'v':
            return checkKeyword(1, 2, "ar", TK_VAR);
        case 'w':
            return checkKeyword(1, 4, "hile", TK_WHILE);
    }

    return TK_IDENTIFIER;
}

/**
 * Extracts an identifier from the source code. A valid identifier contains alphas and digits (only
 * after the first alpha).
 */
static Token identifier() {
    while (isAlpha(peek()) || isDigit(peek())) advance();
    return makeToken(findType());
}

/**
 * Extracts a numeric value from the source code.
 */
static Token number() {
    while (isDigit(peek())) advance();

    if (peek() == '.' && isDigit(peekNext())) { /* Fractional number */
        advance();
        while (isDigit(peek())) advance();
    }

    return makeToken(TK_NUMBER);
}

/**
 * Extracts a string value from the source code.
 */
static Token string() {
    while (peek() != '"' && !reachedEOF()) {
        if (peek() == '\n') scanner.line++;
        advance();
    }

    if (reachedEOF()) /* Checks if is an unterminated string */
        return errorToken("Unterminated string.");

    advance();
    return makeToken(TK_STRING);
}

/**
 * Main scanner function. Scans and returns the next token in the source code.
 */
Token scanToken() {
    preProcessSource();              /* Handles unnecessary characters */
    scanner.start = scanner.current; /* Sets the start point to the last token */
    if (reachedEOF()) return makeToken(TK_EOF);

    char nextChar = advance();                  /* Gets the next character */
    if (isAlpha(nextChar)) return identifier(); /* Checks if is an alpha */
    if (isDigit(nextChar)) return number();     /* Checks if is an digit */

    switch (nextChar) { /* Checks for lexemes matching */
        case '(':
            return makeToken(TK_LEFT_PAREN);
        case ')':
            return makeToken(TK_RIGHT_PAREN);
        case '{':
            return makeToken(TK_LEFT_BRACE);
        case '}':
            return makeToken(TK_RIGHT_BRACE);
        case ';':
            return makeToken(TK_SEMICOLON);
        case ',':
            return makeToken(TK_COMMA);
        case '.':
            return makeToken(TK_DOT);
        case '-':
            if (match('-'))
                return makeToken(TK_DECREMENT);
            else if (match('='))
                return makeToken(TK_MINUS_EQUAL);
            else
                return makeToken(TK_MINUS);
        case '+':
            if (match('+'))
                return makeToken(TK_INCREMENT);
            else if (match('='))
                return makeToken(TK_PLUS_EQUAL);
            else
                return makeToken(TK_PLUS);
        case '/':
            return makeToken(match('=') ? TK_DIV_EQUAL : TK_DIV);
        case '*':
            return makeToken(match('=') ? TK_MULTIPLY_EQUAL : TK_MULTIPLY);
        case '%':
            return makeToken(match('=') ? TK_MOD_EQUAL : TK_MOD);
        case '!':
            return makeToken(match('=') ? TK_NOT_EQUAL : TK_NOT);
        case '=':
            return makeToken(match('=') ? TK_EQUAL_EQUAL : TK_EQUAL);
        case '<':
            return makeToken(match('=') ? TK_LESS_EQUAL : TK_LESS);
        case '>':
            return makeToken(match('=') ? TK_GREATER_EQUAL : TK_GREATER);
        case '"':
            return string();
        default:
            return errorToken("Unexpected character.");
    }
}
