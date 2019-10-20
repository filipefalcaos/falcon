/*
 * YAPL version 0.0.1 (master, Oct 18 2019)
 * yapl_compiler.c: YAPL's handwritten parser/compiler based on Pratt Parsing
 * See YAPL's license in the LICENSE file
 */

#include "../include/yapl_compiler.h"
#include "../include/commons.h"
#include "../include/yapl_object.h"
#include "../include/yapl_scanner.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef YAPL_DEBUG_PRINT_CODE
#include "../include/yapl_debug.h"
#endif

/* YAPL's parser representation */
typedef struct {
    Token current;
    Token previous;
    bool hadError;
    bool panicMode;
} Parser;

/* YAPL's precedence levels, from lowest to highest */
typedef enum {
    PREC_NONE,
    PREC_ASSIGN,  /* '=', '+=', '-=', '*=', '/=', '%=' */
    PREC_OR,      /* 'or' */
    PREC_AND,     /* 'and' */
    PREC_EQUAL,   /* '==', '!=' */
    PREC_COMPARE, /* '<', '>', '<=', '>=' */
    PREC_TERM,    /* '+', '-' */
    PREC_FACTOR,  /* '*', '/', '%' */
    PREC_UNARY,   /* '!', '-', '++', '--' */
    PREC_POSTFIX, /* '.', '()', '[]' */
    PREC_PRIMARY
} Precedence;

/* Function pointer to parse functions */
typedef void (*ParseFunction)();

/* YAPL's parsing rules (prefix function, infix function, precedence function) */
typedef struct {
    ParseFunction prefix;
    ParseFunction infix;
    Precedence precedence;
} ParseRule;

/* Parser instance */
Parser parser;
BytecodeChunk *compilingBytecodeChunk;

/**
 * Returns the compiling bytecode chunk.
 */
static BytecodeChunk *currentBytecodeChunk() { return compilingBytecodeChunk; }

/**
 * Presents a syntax/compiler error to the programmer.
 */
void compilerError(Token *token, const char *message, ...) {
    va_list args;
    va_start(args, message); /* Gets the arguments */
    va_end(args);

    /* Checks and sets error recovery */
    if (parser.panicMode) return;
    parser.panicMode = true;

    fprintf(stderr, "[%d:%d] Syntax error", token->line, token->column); /* Prints the line */
    switch (token->type) {
        case TK_ERROR:
            break;
        case TK_EOF:
            fprintf(stderr, ": ");
            break;
        default:
            fprintf(stderr, " at '%.*s': ", token->length, token->start); /* Prints the error */
    }

    vfprintf(stderr, message, args); /* Prints the error message */
    fprintf(stderr, "\n");
    parser.hadError = true;
}

/**
 * Takes the old current token and then loops through the token stream and to get the next token.
 * The loop keeps going reading tokens and reporting the errors, until it hits a non-error one or
 * reach EOF.
 */
static void advance() {
    parser.previous = parser.current;

    while (true) {
        parser.current = scanToken();
        if (parser.current.type != TK_ERROR) break;
        compilerError(&parser.current, parser.current.start);
    }
}

/**
 * Reads the next token and validates that the token has an expected type. If not, reports an error.
 */
static void consume(TokenType type, const char *message) {
    if (parser.current.type == type) {
        advance();
        return;
    }

    compilerError(&parser.current, message);
}

/**
 * Appends a single byte to the bytecode chunk.
 */
static void emitByte(uint8_t byte) {
    writeToBytecodeChunk(currentBytecodeChunk(), byte, parser.previous.line);
}

/**
 * Appends two bytes (operation code and operand) to the bytecode chunk.
 */
static void emitBytes(uint8_t byte_1, uint8_t byte_2) {
    emitByte(byte_1);
    emitByte(byte_2);
}

/**
 * Emits the OP_RETURN bytecode instruction.
 */
static void emitReturn() { emitByte(OP_RETURN); }

/**
 * Inserts a constant in the constants table of the current bytecode chunk. Before inserting,
 * checks if the constant limit was exceeded.
 */
static void emitConstant(Value value) {
    uint16_t constant = writeConstant(currentBytecodeChunk(), value, parser.previous.line);
    if (constant > UINT16_MAX) compilerError(&parser.previous, "Too many constants in one chunk.");
}

/**
 * Ends the compilation process.
 */
static void endCompiler() {
    emitReturn();
#ifdef YAPL_DEBUG_PRINT_CODE
    if (!parser.hadError) {
        printOpcodesHeader();
        disassembleBytecodeChunk(currentBytecodeChunk(), "Source code");
#ifdef YAPL_DEBUG_TRACE_EXECUTION
        printf("\n");
#endif
    }
#endif
}

/* Forward parser's declarations, since the grammar is recursive */
static void expression();
static ParseRule *getRule(TokenType type);
static void parsePrecedence(Precedence precedence);

/**
 * Handles a binary (infix) expression, by compiling the right operand of the expression (the left
 * one was already compiled). Then, emits the bytecode instruction that performs the binary
 * operation.
 */
static void binary() {
    TokenType operatorType = parser.previous.type;
    ParseRule *rule = getRule(operatorType); /* Gets the current rule */
    parsePrecedence(rule->precedence + 1);   /* Compiles with the correct precedence */

    /* Emits the operator instruction */
    switch (operatorType) {
        case TK_NOT_EQUAL:
            emitBytes(OP_EQUAL, OP_NOT);
            break;
        case TK_EQUAL_EQUAL:
            emitByte(OP_EQUAL);
            break;
        case TK_GREATER:
            emitByte(OP_GREATER);
            break;
        case TK_GREATER_EQUAL:
            emitBytes(OP_LESS, OP_NOT);
            break;
        case TK_LESS:
            emitByte(OP_LESS);
            break;
        case TK_LESS_EQUAL:
            emitBytes(OP_GREATER, OP_NOT);
            break;
        case TK_PLUS:
            emitByte(OP_ADD);
            break;
        case TK_MINUS:
            emitByte(OP_SUBTRACT);
            break;
        case TK_DIV:
            emitByte(OP_DIVIDE);
            break;
        case TK_MOD:
            emitByte(OP_MOD);
            break;
        case TK_MULTIPLY:
            emitByte(OP_MULTIPLY);
            break;
        default:
            return; /* Unreachable */
    }
}

/**
 * Handles a literal (booleans or null) expression by outputting the proper instruction.
 */
static void literal() {
    switch (parser.previous.type) {
        case TK_FALSE:
            emitByte(OP_FALSE);
            break;
        case TK_NULL:
            emitByte(OP_NULL);
            break;
        case TK_TRUE:
            emitByte(OP_TRUE);
            break;
        default:
            return; /* Unreachable */
    }
}

/**
 * Handles the opening parenthesis by compiling the expression between the parentheses, and then
 * parsing the closing parenthesis.
 */
static void grouping() {
    expression();
    consume(TK_RIGHT_PAREN, "Expected ')' after expression.");
}

/**
 * Handles a numeric expression by converting a string to a double number and then generates the
 * code to load that value by calling 'emitConstant'.
 */
static void number() {
    double value = strtod(parser.previous.start, NULL);
    emitConstant(NUMBER_VAL(value));
}

/**
 * Handles a string expression by creating a string object, wrapping it in a Value, and then
 * adding it to the constants table.
 */
static void string() {
    emitConstant(OBJ_VAL(copyString(parser.previous.start + 1, parser.previous.length - 2)));
}

/**
 * Handles a unary expression by compiling the operand and then emitting the bytecode to perform
 * the unary operation itself.
 */
static void unary() {
    TokenType operatorType = parser.previous.type;
    parsePrecedence(PREC_UNARY); /* Compiles the operand */

    switch (operatorType) {
        case TK_MINUS:
            emitByte(OP_NEGATE);
            break;
        case TK_NOT:
            emitByte(OP_NOT);
            break;
        default:
            return;
    }
}

/* An array (table) of ParseRules that guides the Pratt parser. The left column represents the
 * prefix function parser; the second column represents the infix function parser; and the third
 * column represents the precedence level */
#define EMPTY_RULE \
    { NULL, NULL, PREC_NONE }
#define PREFIX_RULE(prefix) \
    { prefix, NULL, PREC_NONE }
#define INFIX_RULE(infix, prec) \
    { NULL, infix, prec }
#define RULE(prefix, infix, prec) \
    { prefix, infix, prec }

ParseRule rules[] = {
    PREFIX_RULE(grouping),            /* TK_LEFT_PAREN */
    EMPTY_RULE,                       /* TK_RIGHT_PAREN */
    EMPTY_RULE,                       /* TK_LEFT_BRACE */
    EMPTY_RULE,                       /* TK_RIGHT_BRACE */
    EMPTY_RULE,                       /* TK_COMMA */
    EMPTY_RULE,                       /* TK_DOT */
    EMPTY_RULE,                       /* TK_SEMICOLON */
    RULE(unary, binary, PREC_TERM),   /* TK_MINUS */
    INFIX_RULE(binary, PREC_TERM),    /* TK_PLUS */
    INFIX_RULE(binary, PREC_FACTOR),  /* TK_DIV */
    INFIX_RULE(binary, PREC_FACTOR),  /* TK_MOD */
    INFIX_RULE(binary, PREC_FACTOR),  /* TK_MULTIPLY */
    PREFIX_RULE(unary),               /* TK_NOT */
    INFIX_RULE(binary, PREC_EQUAL),   /* TK_NOT_EQUAL */
    EMPTY_RULE,                       /* TK_EQUAL */
    INFIX_RULE(binary, PREC_EQUAL),   /* TK_EQUAL_EQUAL */
    INFIX_RULE(binary, PREC_COMPARE), /* TK_GREATER */
    INFIX_RULE(binary, PREC_COMPARE), /* TK_GREATER_EQUAL */
    INFIX_RULE(binary, PREC_COMPARE), /* TK_LESS */
    INFIX_RULE(binary, PREC_COMPARE), /* TK_LESS_EQUAL */
    EMPTY_RULE,                       /* TK_DECREMENT */
    EMPTY_RULE,                       /* TK_INCREMENT */
    EMPTY_RULE,                       /* TK_PLUS_EQUAL */
    EMPTY_RULE,                       /* TK_MINUS_EQUAL */
    EMPTY_RULE,                       /* TK_MULTIPLY_EQUAL */
    EMPTY_RULE,                       /* TK_DIV_EQUAL */
    EMPTY_RULE,                       /* TK_MOD_EQUAL */
    EMPTY_RULE,                       /* TK_AND */
    EMPTY_RULE,                       /* TK_OR */
    EMPTY_RULE,                       /* TK_IDENTIFIER */
    PREFIX_RULE(string),              /* TK_STRING */
    PREFIX_RULE(number),              /* TK_NUMBER */
    EMPTY_RULE,                       /* TK_CLASS */
    EMPTY_RULE,                       /* TK_ELSE */
    PREFIX_RULE(literal),             /* TK_FALSE */
    EMPTY_RULE,                       /* TK_FOR */
    EMPTY_RULE,                       /* TK_FUNCTION */
    EMPTY_RULE,                       /* TK_IF */
    PREFIX_RULE(literal),             /* TK_NULL */
    EMPTY_RULE,                       /* TK_PRINT */
    EMPTY_RULE,                       /* TK_PUTS */
    EMPTY_RULE,                       /* TK_RETURN */
    EMPTY_RULE,                       /* TK_SUPER */
    EMPTY_RULE,                       /* TK_THIS */
    PREFIX_RULE(literal),             /* TK_TRUE */
    EMPTY_RULE,                       /* TK_UNLESS */
    EMPTY_RULE,                       /* TK_VAR */
    EMPTY_RULE,                       /* TK_WHILE */
    EMPTY_RULE,                       /* TK_ERROR */
    EMPTY_RULE                        /* TK_EOF */
};

#undef EMPTY_RULE
#undef PREFIX_RULE
#undef INFIX_RULE
#undef RULE

/**
 * Parses any expression of a given precedence level or higher. Reads the next token and looks up
 * the corresponding ParseRule. If there is no prefix parser, then the token must be a syntax error.
 */
static void parsePrecedence(Precedence precedence) {
    advance();
    ParseFunction prefixRule = getRule(parser.previous.type)->prefix;
    if (prefixRule == NULL) {
        compilerError(&parser.previous, "Expected expression.");
        return;
    }

    prefixRule();
    while (precedence <= getRule(parser.current.type)->precedence) {
        advance();
        ParseFunction infixRule = getRule(parser.previous.type)->infix;
        infixRule();
    }
}

/**
 * Returns the rule at a given index.
 */
static ParseRule *getRule(TokenType type) { return &rules[type]; }

/**
 * Compiles an expression by parsing the lowest precedence level, which subsumes all of the higher
 * precedence expressions too.
 */
void expression() { parsePrecedence(PREC_ASSIGN); }

/**
 * Compiles a given source code string. The parsing technique used is a Pratt parser, an improved
 * recursive descent parser that associates semantics with tokens instead of grammar rules.
 */
bool compile(const char *source, BytecodeChunk *bytecodeChunk) {
    parser.hadError = false;
    parser.panicMode = false;
    compilingBytecodeChunk = bytecodeChunk;
    initScanner(source);
    advance();
    expression();
    consume(TK_EOF, "Expected end of expression.");
    endCompiler();
    return !parser.hadError;
}
