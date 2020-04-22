/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * fl-scanner.c: Falcons's handwritten Scanner
 * See Falcon's license in the LICENSE file
 */

#include "fl-scanner.h"
#include "../lib/fl-strlib.h"
#include "fl-mem.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>

/**
 * Initializes the scanner with the first character of the first source code line.
 */
void init_scanner(const char *source, Scanner *scanner) {
    scanner->start = source;
    scanner->current = source;
    scanner->source = source;
    scanner->line = 1;
    scanner->column = 0;
}

/**
 * Gets the current line in the scanner.
 */
const char *get_current_line(Scanner *scanner) { return scanner->source; }

/**
 * Check if a character is a valid alpha.
 */
static bool is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

/**
 * Check if a character is a digit.
 */
static bool is_digit(char c) { return c >= '0' && c <= '9'; }

/**
 * Check if the scanner reached the EOF.
 */
static bool has_reached_EOF(Scanner *scanner) { return *scanner->current == '\0'; }

/**
 * Advance the scanner to read the next character from the input source code.
 */
static char advance(Scanner *scanner) {
    scanner->current++;
    scanner->column++;
    return scanner->current[-1];
}

/**
 * Gets the current character in the scanner.
 */
static char peek(Scanner *scanner) { return *scanner->current; }

/**
 * Gets the next character in the scanner, without advancing.
 */
static char peek_next(Scanner *scanner) {
    if (has_reached_EOF(scanner)) return '\0';
    return scanner->current[1];
}

/**
 * Checks if a character matches the current one on the scanner. If so, advances to the next
 * character in the scanner.
 */
static bool match(char expected, Scanner *scanner) {
    if (has_reached_EOF(scanner) || peek(scanner) != expected) {
        return false;
    } else {
        scanner->current++;
        scanner->column++;
        return true;
    }
}

/**
 * Makes a new token using the current lexeme in the scanner.
 */
static Token make_token(Token token, Scanner *scanner) {
    token.start = scanner->start;
    token.length = (uint32_t)(scanner->current - scanner->start);
    token.line = scanner->line;
    token.column = scanner->column;
    return token;
}

/**
 * Makes a simple token (i.e., non-literal tokens) of a certain token type.
 */
static Token simple_token(FalconTokens type, Scanner *scanner) {
    Token token;
    token.type = type;
    return make_token(token, scanner);
}

/**
 * Makes a token of a number or string literal.
 */
static Token literal_token(FalconTokens type, FalconValue value, Scanner *scanner) {
    Token token;
    token.type = type;
    token.value = value;
    return make_token(token, scanner);
}

/**
 * Makes the error token (TK_ERROR) with a message to present to the programmer.
 */
static Token error_token(const char *message, Scanner *scanner) {
    Token token;
    token.type = TK_ERROR;
    token.start = message;
    token.length = (uint32_t) strlen(message);
    token.line = scanner->line;
    token.column = scanner->column;
    return token;
}

/**
 * Handles unnecessary characters in the input source code. When a whitespace, a tab, a new line,
 * a carriage return, or a comment is the current character, the scanner advances to the next one.
 */
static void clean_source(Scanner *scanner) {
    while (true) {
        char c = peek(scanner);
        switch (c) {
            case ' ':
            case '\r':
            case '\t': advance(scanner); break;
            case '\n':
                scanner->line++;
                advance(scanner);
                scanner->column = 0;
                scanner->source = scanner->current;
                break;
            case '#':
                while (peek(scanner) != '\n' && !has_reached_EOF(scanner))
                    advance(scanner); /* Loop on comments */
                break;
            default: return;
        }
    }
}

/**
 * Checks if a name matches a keyword. To match, the lexeme must be exactly as long as the keyword
 * and the remaining characters must match exactly.
 */
static FalconTokens check_keyword(int start, int length, const char *rest, FalconTokens type,
                                  Scanner *scanner) {
    if (scanner->current - scanner->start == start + length &&
        memcmp(scanner->start + start, rest, (size_t) length) == 0)
        return type;
    return TK_IDENTIFIER;
}

/**
 * Checks if a name is an identifier or a defined Falcon keyword.
 */
static FalconTokens find_token_type(Scanner *scanner) {
    const char start = scanner->start[0]; /* Current char */

    switch (start) {
        case 'a': return check_keyword(1, 2, "nd", TK_AND, scanner);
        case 'b': return check_keyword(1, 4, "reak", TK_BREAK, scanner);
        case 'c': return check_keyword(1, 4, "lass", TK_CLASS, scanner);
        case 'e': return check_keyword(1, 3, "lse", TK_ELSE, scanner);
        case 'f':
            if (scanner->current - scanner->start > 1) {
                switch (scanner->start[1]) {
                    case 'a': return check_keyword(2, 3, "lse", TK_FALSE, scanner);
                    case 'n': return check_keyword(2, 0, "", TK_FUNCTION, scanner);
                    case 'o': return check_keyword(2, 1, "r", TK_FOR, scanner);
                    default: break;
                }
            }
            break;
        case 'i': return check_keyword(1, 1, "f", TK_IF, scanner);
        case 'n':
            if (scanner->current - scanner->start > 1) {
                switch (scanner->start[1]) {
                    case 'e': return check_keyword(2, 2, "xt", TK_NEXT, scanner);
                    case 'o': return check_keyword(2, 1, "t", TK_NOT, scanner);
                    case 'u': return check_keyword(2, 2, "ll", TK_NULL, scanner);
                    default: break;
                }
            }
            break;
        case 'o': return check_keyword(1, 1, "r", TK_OR, scanner);
        case 'r': return check_keyword(1, 5, "eturn", TK_RETURN, scanner);
        case 's':
            if (scanner->current - scanner->start > 1) {
                switch (scanner->start[1]) {
                    case 'u': return check_keyword(2, 3, "per", TK_SUPER, scanner);
                    case 'w': return check_keyword(2, 4, "itch", TK_SWITCH, scanner);
                    default: break;
                }
            }
            break;
        case 't':
            if (scanner->current - scanner->start > 1) {
                switch (scanner->start[1]) {
                    case 'h': return check_keyword(2, 2, "is", TK_THIS, scanner);
                    case 'r': return check_keyword(2, 2, "ue", TK_TRUE, scanner);
                    default: break;
                }
            }
            break;
        case 'v': return check_keyword(1, 2, "ar", TK_VAR, scanner);
        case 'w': {
            const char *current = scanner->start;
            long length = scanner->current - scanner->start; /* Current token length */

            if (length > 1) {
                switch (current[1]) {
                    case 'h': {
                        if (length > 2) {
                            switch (current[2]) {
                                case 'e': return check_keyword(3, 1, "n", TK_WHEN, scanner);
                                case 'i': return check_keyword(3, 2, "le", TK_WHILE, scanner);
                                default: break;
                            }
                        }
                    }
                    default: break;
                }
            }

            break;
        }
        default: return TK_IDENTIFIER;
    }

    return TK_IDENTIFIER;
}

/**
 * Extracts an identifier from the source code. A valid identifier contains alphas and digits (only
 * after the first alpha).
 */
static Token identifier(Scanner *scanner) {
    while (is_alpha(peek(scanner)) || is_digit(peek(scanner))) advance(scanner);
    return simple_token(find_token_type(scanner), scanner);
}

/**
 * Extracts a numeric value from the source code.
 */
static Token number(Scanner *scanner) {
    while (is_digit(peek(scanner))) advance(scanner);

    /* Fractional number */
    if (peek(scanner) == '.' && is_digit(peek_next(scanner))) {
        advance(scanner);
        while (is_digit(peek(scanner))) advance(scanner);
    }

    /* Gets the numeric value of the token */
    double numValue = strtod(scanner->start, NULL);
    if (errno == ERANGE) {
        error_token(SCAN_BIG_NUM_ERR, scanner);
        numValue = 0;
        errno = 0;
    }

    FalconValue value = NUM_VAL(numValue);
    return literal_token(TK_NUMBER, value, scanner); /* Makes a number literal token */
}

/**
 * Extracts a string value from the source code. The string is dynamically allocated to handle
 * escape characters and string interpolation.
 */
static Token string(Scanner *scanner, FalconVM *vm) {
    uint64_t currentSize = 0;
    char *string = FALCON_ALLOCATE(vm, char, currentSize); /* Allocates initial space */

    while (true) {
        char nextChar = advance(scanner);

        if (nextChar == '"') /* Checks if the string is terminated */
            break;           /* Goes to "FalconValue value = ..." */

        if (nextChar == '\0') /* Checks if is an unterminated string */
            return error_token(SCAN_UNTERMINATED_STR_ERR, scanner);

        /* Newlines inside strings are ignored */
        if (nextChar == '\n' || nextChar == '\r') {
            scanner->line++;
            scanner->column = 0;
            scanner->source = scanner->current;
        }

        /* Checks if an escape character is found */
        if (nextChar == '\\') {
            switch (advance(scanner)) {
                case '"': nextChar = '"'; break;
                case '\\': nextChar = '\\'; break;
                case 'b': nextChar = '\b'; break;
                case 'n': nextChar = '\n'; break;
                case 'r': nextChar = '\r'; break;
                case 'f': nextChar = '\f'; break;
                case 't': nextChar = '\t'; break;
                case 'v': nextChar = '\v'; break;
                default: return error_token(SCAN_INVALID_ESCAPE, scanner);
            }
        }

        /* Adds the character to the string */
        uint64_t oldSize = currentSize;
        currentSize++;
        string = reallocate(vm, string, oldSize, currentSize); /* Increases the string */
        string[oldSize] = nextChar;
    }

    FalconValue value = OBJ_VAL(new_ObjString(vm, string, currentSize));
    return literal_token(TK_STRING, value, scanner); /* Makes a string literal token */
}

/**
 * Main scanner function. Scans and returns the next token in the source code.
 */
Token scan_token(Scanner *scanner, FalconVM *vm) {
    clean_source(scanner);             /* Handles unnecessary characters */
    scanner->start = scanner->current; /* Sets the start point to the last token */
    if (has_reached_EOF(scanner)) return simple_token(TK_EOF, scanner);

    char nextChar = advance(scanner);                   /* Gets the next character */
    if (is_alpha(nextChar)) return identifier(scanner); /* Checks if is an alpha */
    if (is_digit(nextChar)) return number(scanner);     /* Checks if is an digit */

    switch (nextChar) { /* Checks for lexemes matching */
        case '(': return simple_token(TK_LPAREN, scanner);
        case ')': return simple_token(TK_RPAREN, scanner);
        case '{': return simple_token(TK_LBRACE, scanner);
        case '}': return simple_token(TK_RBRACE, scanner);
        case '[': return simple_token(TK_LBRACKET, scanner);
        case ']': return simple_token(TK_RBRACKET, scanner);
        case '?': return simple_token(TK_QUESTION, scanner);
        case ':': return simple_token(TK_COLON, scanner);
        case ';': return simple_token(TK_SEMICOLON, scanner);
        case ',': return simple_token(TK_COMMA, scanner);
        case '.': return simple_token(TK_DOT, scanner);
        case '-':
            if (match('>', scanner)) {
                return simple_token(TK_ARROW, scanner);
            } else {
                return simple_token(TK_MINUS, scanner);
            }
        case '+': return simple_token(TK_PLUS, scanner);
        case '/': return simple_token(TK_SLASH, scanner);
        case '%': return simple_token(TK_PERCENT, scanner);
        case '*': return simple_token(TK_STAR, scanner);
        case '^': return simple_token(TK_CIRCUMFLEX, scanner);
        case '!':
            if (match('=', scanner)) /* Logical not operator is "not" instead of "!" */
                return simple_token(TK_NOTEQUAL, scanner);
        case '=': return simple_token(match('=', scanner) ? TK_EQEQUAL : TK_EQUAL, scanner);
        case '<': return simple_token(match('=', scanner) ? TK_LESSEQUAL : TK_LESS, scanner);
        case '>': return simple_token(match('=', scanner) ? TK_GREATEREQUAL : TK_GREATER, scanner);
        case '"': return string(scanner, vm);
        default: return error_token(SCAN_UNEXPECTED_TK_ERR, scanner);
    }
}
