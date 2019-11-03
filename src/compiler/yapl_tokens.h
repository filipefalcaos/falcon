/*
 * YAPL version 0.0.1 (master, Oct 18 2019)
 * yapl_tokens.h: YAPL's list of tokens
 * See YAPL's license in the LICENSE file
 */

#ifndef YAPL_TOKENS_H
#define YAPL_TOKENS_H

/* Types of tokens in YAPL */
typedef enum {

    /* Symbols */
    TK_LEFT_PAREN,
    TK_RIGHT_PAREN,
    TK_LEFT_BRACE,
    TK_RIGHT_BRACE,
    TK_COMMA,
    TK_DOT,
    TK_SEMICOLON,
    TK_ARROW,

    /* Operators */
    TK_MINUS,
    TK_PLUS,
    TK_DIV,
    TK_MOD,
    TK_MULTIPLY,
    TK_NOT,
    TK_NOT_EQUAL,
    TK_EQUAL,
    TK_EQUAL_EQUAL,
    TK_GREATER,
    TK_GREATER_EQUAL,
    TK_LESS,
    TK_LESS_EQUAL,
    TK_DECREMENT,
    TK_INCREMENT,
    TK_AND, /* Also a keyword */
    TK_OR,  /* Also a keyword */

    /* Identifier and Literals */
    TK_IDENTIFIER,
    TK_STRING,
    TK_NUMBER,

    /* Keywords */
    TK_CLASS,
    TK_ELSE,
    TK_FALSE, /* Also a literal */
    TK_FOR,
    TK_FUNCTION,
    TK_IF,
    TK_NULL,
    TK_RETURN,
    TK_SUPER,
    TK_SWITCH,
    TK_THIS,
    TK_TRUE, /* Also a literal */
    TK_UNLESS,
    TK_VAR,
    TK_WHEN,
    TK_WHILE,

    /* Error and EOF */
    TK_ERROR,
    TK_EOF

} TokenType;

#endif // YAPL_TOKENS_H
