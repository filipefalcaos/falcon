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
    FALCON_TK_LEFT_PAREN,
    FALCON_TK_RIGHT_PAREN,
    FALCON_TK_LEFT_BRACE,
    FALCON_TK_RIGHT_BRACE,
    FALCON_TK_COMMA,
    FALCON_TK_DOT,
    FALCON_TK_COLON,
    FALCON_TK_SEMICOLON,
    FALCON_TK_ARROW,

    /* Operators */
    FALCON_TK_MINUS,
    FALCON_TK_MINUS_EQUAL,
    FALCON_TK_PLUS,
    FALCON_TK_PLUS_EQUAL,
    FALCON_TK_DIV,
    FALCON_TK_DIV_EQUAL,
    FALCON_TK_MOD,
    FALCON_TK_MOD_EQUAL,
    FALCON_TK_MULTIPLY,
    FALCON_TK_MULTIPLY_EQUAL,
    FALCON_TK_POW,
    FALCON_TK_POW_EQUAL,
    FALCON_TK_NOT,
    FALCON_TK_NOT_EQUAL,
    FALCON_TK_EQUAL,
    FALCON_TK_EQUAL_EQUAL,
    FALCON_TK_GREATER,
    FALCON_TK_GREATER_EQUAL,
    FALCON_TK_LESS,
    FALCON_TK_LESS_EQUAL,
    FALCON_TK_AND, /* Also a keyword */
    FALCON_TK_OR,  /* Also a keyword */
    FALCON_TK_TERNARY,

    /* Identifier and Literals */
    FALCON_TK_IDENTIFIER,
    FALCON_TK_STRING,
    FALCON_TK_NUMBER,

    /* Keywords */
    FALCON_TK_BREAK,
    FALCON_TK_CLASS,
    FALCON_TK_ELSE,
    FALCON_TK_FALSE, /* Also a literal */
    FALCON_TK_FOR,
    FALCON_TK_FUNCTION,
    FALCON_TK_IF,
    FALCON_TK_NEXT,
    FALCON_TK_NULL,
    FALCON_TK_RETURN,
    FALCON_TK_SUPER,
    FALCON_TK_SWITCH,
    FALCON_TK_THIS,
    FALCON_TK_TRUE, /* Also a literal */
    FALCON_TK_VAR,
    FALCON_TK_WHEN,
    FALCON_TK_WHILE,

    /* Error and EOF */
    FALCON_TK_ERROR,
    FALCON_TK_EOF

} FalconTokenType;

#endif // FALCON_TOKENS_H
