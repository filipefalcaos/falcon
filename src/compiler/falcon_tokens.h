/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * falcon_tokens.h: Falcon's list of tokens
 * See Falcon's license in the LICENSE file
 */

#ifndef FALCON_TOKENS_H
#define FALCON_TOKENS_H

/* Types of tokens in Falcon */
typedef enum {

    /* Symbols */
    TK_LEFT_PAREN,
    TK_RIGHT_PAREN,
    TK_LEFT_BRACE,
    TK_RIGHT_BRACE,
    TK_LEFT_BRACKET,
    TK_RIGHT_BRACKET,
    TK_COMMA,
    TK_DOT,
    TK_COLON,
    TK_SEMICOLON,
    TK_ARROW,

    /* Operators */
    TK_MINUS,
    TK_PLUS,
    TK_DIV,
    TK_MOD,
    TK_MULTIPLY,
    TK_POW,
    TK_NOT,
    TK_NOT_EQUAL,
    TK_EQUAL,
    TK_EQUAL_EQUAL,
    TK_GREATER,
    TK_GREATER_EQUAL,
    TK_LESS,
    TK_LESS_EQUAL,
    TK_AND, /* Also a keyword */
    TK_OR,  /* Also a keyword */
    TK_TERNARY,

    /* Identifier and Literals */
    TK_IDENTIFIER,
    TK_STRING,
    TK_NUMBER,

    /* Keywords */
    TK_BREAK,
    TK_CLASS,
    TK_ELSE,
    TK_FALSE, /* Also a literal */
    TK_FOR,
    TK_FUNCTION,
    TK_IF,
    TK_NEXT,
    TK_NULL,
    TK_RETURN,
    TK_SUPER,
    TK_SWITCH,
    TK_THIS,
    TK_TRUE, /* Also a literal */
    TK_VAR,
    TK_WHEN,
    TK_WHILE,

    /* Error and EOF */
    TK_ERROR,
    TK_EOF

} FalconTokens;

#endif // FALCON_TOKENS_H
