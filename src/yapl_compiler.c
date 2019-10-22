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
#include <string.h>

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
typedef void (*ParseFunction)(bool canAssign);

/* YAPL's parsing rules (prefix function, infix function, precedence function) */
typedef struct {
    ParseFunction prefix;
    ParseFunction infix;
    Precedence precedence;
} ParseRule;

/* Local variable representation */
typedef struct {
    Token name;
    int depth;
} Local;

/* YAPL's function types */
typedef enum { TYPE_FUNCTION, TYPE_SCRIPT } FunctionType;

/* YAPL's compiler representation */
typedef struct Compiler {
    struct Compiler *enclosing;
    ObjFunction *function;
    FunctionType type;             // TODO: change to a bitfield?
    Local locals[MAX_SINGLE_BYTE]; // TODO: make it possible to have more than 256
    int localCount;
    int scopeDepth;
} Compiler;

/* Parser instance */
Parser parser;
Table globalNames;
Compiler *current = NULL;

/**
 * Returns the compiling bytecode chunk.
 */
static BytecodeChunk *currentBytecodeChunk() { return &current->function->bytecodeChunk; }

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
 * Checks if the current token if of the given type.
 */
static bool check(TokenType type) { return parser.current.type == type; }

/**
 * Checks if the current token is of the given type. If so, the token is consumed.
 */
static bool match(TokenType type) {
    if (!check(type)) return false;
    advance();
    return true;
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
static void emitReturn() { emitBytes(OP_NULL, OP_RETURN); }

/**
 * Adds a constant to the bytecode chunk constants table.
 */
static uint8_t makeConstant(Value value) {
    int constant = addConstant(currentBytecodeChunk(), value);
    if (constant > UINT8_MAX) {
        compilerError(&parser.previous, "Too many constants in one chunk.");
        return 0;
    }

    return (uint8_t) constant;
}

/**
 * Inserts a constant in the constants table of the current bytecode chunk. Before inserting,
 * checks if the constant limit was exceeded.
 */
static void emitConstant(Value value) {
    uint16_t constant = writeConstant(currentBytecodeChunk(), value, parser.previous.line);
    if (constant > UINT16_MAX) compilerError(&parser.previous, "Too many constants in one chunk.");
}

/**
 * Starts a new compilation process.
 */
static void initCompiler(Compiler *compiler, FunctionType type) {
    compiler->enclosing = current;
    compiler->function = NULL;
    compiler->type = type;
    compiler->localCount = 0;
    compiler->scopeDepth = 0;
    compiler->function = newFunction();
    current = compiler;

    if (type != TYPE_SCRIPT)
        current->function->name =
            copyString(parser.previous.start, parser.previous.length); /* Sets function name */

    /* Set stack slot zero for the VM's internal use */
    Local *local = &current->locals[current->localCount++];
    local->depth = 0;
    local->name.start = "";
    local->name.length = 0;
}

/**
 * Ends the compilation process.
 */
static ObjFunction *endCompiler() {
    emitReturn();
    ObjFunction *function = current->function;

#ifdef YAPL_DEBUG_PRINT_CODE
    if (!parser.hadError) {
        printOpcodesHeader();
        disassembleBytecodeChunk(currentBytecodeChunk(),
                                 function->name != NULL ? function->name->chars : "<script>");
#ifdef YAPL_DEBUG_TRACE_EXECUTION
        printf("\n");
#endif
    }
#endif

    current = current->enclosing;
    return function;
}

/**
 * Begins a new scope by incrementing the current scope depth.
 */
static void beginScope() { current->scopeDepth++; }

/**
 * Ends an existing scope by decrementing the current scope depth and popping all local variables
 * declared in the scope.
 */
static void endScope() {
    int end, start = current->localCount;
    current->scopeDepth--;

    /* Finds the number of locals in the scope */
    while (current->localCount > 0 &&
           current->locals[current->localCount - 1].depth > current->scopeDepth) {
        current->localCount--;
    }

    end = start - current->localCount; /* Number of locals to remove */
    if (end == 1) {                    /* Single variable in scope */
        emitByte(OP_POP);
    } else if (end > 1) { /* More than one variable in scope */
        emitConstant(NUMBER_VAL(end));
        emitByte(OP_POP_N); // TODO: make it work with an operand
    }
}

/* Forward parser's declarations, since the grammar is recursive */
static void expression();
static void statement();
static void declaration();
static ParseRule *getRule(TokenType type);
static void parsePrecedence(Precedence precedence);

/**
 * Checks if an identifier constant was already defined. If so, return its index. If not, set the
 * identifier in the global names table.
 */
static uint8_t identifierConstant(Token *name) {
    return makeConstant(OBJ_VAL(copyString(name->start, name->length)));
}

/**
 * Checks if two identifiers match.
 */
static bool identifiersEqual(Token *a, Token *b) {
    if (a->length != b->length) return false;
    return memcmp(a->start, b->start, a->length) == 0;
}

/**
 * Resolves a local variable by looping through the list of locals that are currently in the scope.
 * If one has the same name as the identifier token, the variable is resolved.
 */
static int resolveLocal(Compiler *compiler, Token *name) {
    for (int i = compiler->localCount - 1; i >= 0; i--) {
        Local *local = &compiler->locals[i];
        if (identifiersEqual(name, &local->name)) { /* Checks if identifier matches */
            if (local->depth == -1)
                compilerError(&parser.previous, "Cannot read variable in its own initializer.");
            return i;
        }
    }

    return -1;
}

/**
 * Adds a local variable to the list of variables in a scope depth.
 */
static void addLocal(Token name) {
    if (current->localCount == MAX_SINGLE_BYTE) {
        compilerError(&parser.previous, "Too many local variables in scope.");
        return;
    }

    Local *local = &current->locals[current->localCount++];
    local->name = name;
    local->depth = -1;
}

/**
 * Records the existence of a variable declaration.
 */
static void declareVariable() {
    if (current->scopeDepth == 0) return; /* Globals are late bound, exit */
    Token *name = &parser.previous;

    /* Verifies if variable was previously declared */
    for (int i = current->localCount - 1; i >= 0; i--) {
        Local *local = &current->locals[i];
        if (local->depth != -1 && local->depth < current->scopeDepth) break;
        if (identifiersEqual(name, &local->name)) /* Checks if already declared */
            compilerError(&parser.previous, "Variable already declared in this scope.");
    }

    addLocal(*name);
}

/**
 * Parses a variable by consuming an identifier token and then adds the the token lexeme to the
 * constants table.
 */
static uint8_t parseVariable(const char *errorMessage) {
    consume(TK_IDENTIFIER, errorMessage);
    declareVariable();                     /* Declares the variables */
    if (current->scopeDepth > 0) return 0; /* Locals are not looked up by name, exit */
    return identifierConstant(&parser.previous);
}

/**
 * Marks a local variable as initialized and available for use.
 */
static void markInitialized() {
    if (current->scopeDepth == 0) return;
    current->locals[current->localCount - 1].depth = current->scopeDepth;
}

/**
 * Handles a variable declaration by emitting the bytecode to perform a global variable declaration.
 */
static void defineVariable(uint8_t global) {
    if (current->scopeDepth > 0) {
        markInitialized(); /* Mark as initialized */
        return;            /* Only globals are defined at runtime */
    }

    emitBytes(OP_DEFINE_GLOBAL, global);
}

/**
 * Compiles the list of arguments in a function call.
 */
static uint8_t argumentList() {
    uint8_t argCount = 0;

    if (!check(TK_RIGHT_PAREN)) {
        do {
            expression();
            if (argCount == 255)
                compilerError(&parser.previous, "Cannot have more than 255 arguments.");
            argCount++;
        } while (match(TK_COMMA));
    }

    consume(TK_RIGHT_PAREN, "Expected a ')' after arguments.");
    return argCount;
}

/**
 * Handles a binary (infix) expression, by compiling the right operand of the expression (the left
 * one was already compiled). Then, emits the bytecode instruction that performs the binary
 * operation.
 */
static void binary(bool canAssign) {
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
 * Handles a function call expression by parsing its arguments list and emitting the instruction to
 * proceed with the execution of the function.
 */
static void call(bool canAssign) {
    uint8_t argCount = argumentList();
    emitBytes(OP_CALL, argCount);
}

/**
 * Handles a literal (booleans or null) expression by outputting the proper instruction.
 */
static void literal(bool canAssign) {
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
static void grouping(bool canAssign) {
    expression();
    consume(TK_RIGHT_PAREN, "Expected ')' after expression.");
}

/**
 * Handles a numeric expression by converting a string to a double number and then generates the
 * code to load that value by calling "emitConstant".
 */
static void number(bool canAssign) {
    double value = strtod(parser.previous.start, NULL);
    emitConstant(NUMBER_VAL(value));
}

/**
 * Handles a string expression by creating a string object, wrapping it in a Value, and then
 * adding it to the constants table.
 */
static void string(bool canAssign) {
    emitConstant(OBJ_VAL(copyString(parser.previous.start + 1, parser.previous.length - 2)));
}

/**
 * Gets the index of a variable in the constants table and emits the the bytecode to perform the
 * load of the global/local variable.
 */
static void namedVariable(Token name, bool canAssign) {
    uint8_t getOpcode, setOpcode;
    int arg = resolveLocal(current, &name);

    if (arg != -1) {
        getOpcode = OP_GET_LOCAL;
        setOpcode = OP_SET_LOCAL;
    } else {
        arg = identifierConstant(&name);
        getOpcode = OP_GET_GLOBAL;
        setOpcode = OP_SET_GLOBAL;
    }

    if (canAssign && match(TK_EQUAL)) {
        expression();
        emitBytes(setOpcode, (uint8_t) arg);
    } else {
        emitBytes(getOpcode, (uint8_t) arg);
    }
}

/**
 * Handles a variable access.
 */
static void variable(bool canAssign) { namedVariable(parser.previous, canAssign); }

/**
 * Handles a unary expression by compiling the operand and then emitting the bytecode to perform
 * the unary operation itself.
 */
static void unary(bool canAssign) {
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
    RULE(grouping, call, PREC_POSTFIX), /* TK_LEFT_PAREN */
    EMPTY_RULE,                         /* TK_RIGHT_PAREN */
    EMPTY_RULE,                         /* TK_LEFT_BRACE */
    EMPTY_RULE,                         /* TK_RIGHT_BRACE */
    EMPTY_RULE,                         /* TK_COMMA */
    EMPTY_RULE,                         /* TK_DOT */
    EMPTY_RULE,                         /* TK_SEMICOLON */
    EMPTY_RULE,                         /* TK_NL */
    RULE(unary, binary, PREC_TERM),     /* TK_MINUS */
    INFIX_RULE(binary, PREC_TERM),      /* TK_PLUS */
    INFIX_RULE(binary, PREC_FACTOR),    /* TK_DIV */
    INFIX_RULE(binary, PREC_FACTOR),    /* TK_MOD */
    INFIX_RULE(binary, PREC_FACTOR),    /* TK_MULTIPLY */
    PREFIX_RULE(unary),                 /* TK_NOT */
    INFIX_RULE(binary, PREC_EQUAL),     /* TK_NOT_EQUAL */
    EMPTY_RULE,                         /* TK_EQUAL */
    INFIX_RULE(binary, PREC_EQUAL),     /* TK_EQUAL_EQUAL */
    INFIX_RULE(binary, PREC_COMPARE),   /* TK_GREATER */
    INFIX_RULE(binary, PREC_COMPARE),   /* TK_GREATER_EQUAL */
    INFIX_RULE(binary, PREC_COMPARE),   /* TK_LESS */
    INFIX_RULE(binary, PREC_COMPARE),   /* TK_LESS_EQUAL */
    EMPTY_RULE,                         /* TK_DECREMENT */
    EMPTY_RULE,                         /* TK_INCREMENT */
    EMPTY_RULE,                         /* TK_PLUS_EQUAL */
    EMPTY_RULE,                         /* TK_MINUS_EQUAL */
    EMPTY_RULE,                         /* TK_MULTIPLY_EQUAL */
    EMPTY_RULE,                         /* TK_DIV_EQUAL */
    EMPTY_RULE,                         /* TK_MOD_EQUAL */
    EMPTY_RULE,                         /* TK_AND */
    EMPTY_RULE,                         /* TK_OR */
    PREFIX_RULE(variable),              /* TK_IDENTIFIER */
    PREFIX_RULE(string),                /* TK_STRING */
    PREFIX_RULE(number),                /* TK_NUMBER */
    EMPTY_RULE,                         /* TK_CLASS */
    EMPTY_RULE,                         /* TK_ELSE */
    PREFIX_RULE(literal),               /* TK_FALSE */
    EMPTY_RULE,                         /* TK_FOR */
    EMPTY_RULE,                         /* TK_FUNCTION */
    EMPTY_RULE,                         /* TK_IF */
    PREFIX_RULE(literal),               /* TK_NULL */
    EMPTY_RULE,                         /* TK_RETURN */
    EMPTY_RULE,                         /* TK_SUPER */
    EMPTY_RULE,                         /* TK_THIS */
    PREFIX_RULE(literal),               /* TK_TRUE */
    EMPTY_RULE,                         /* TK_UNLESS */
    EMPTY_RULE,                         /* TK_VAR */
    EMPTY_RULE,                         /* TK_WHILE */
    EMPTY_RULE,                         /* TK_ERROR */
    EMPTY_RULE                          /* TK_EOF */
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

    if (prefixRule == NULL) { /* Checks if is a parsing error */
        compilerError(&parser.previous, "Expected expression.");
        return;
    }

    /* Checks if the left side is assignable */
    bool canAssign = precedence <= PREC_ASSIGN;
    prefixRule(canAssign);

    /* Looks for an infix parser for the next token */
    while (precedence <= getRule(parser.current.type)->precedence) {
        advance();
        ParseFunction infixRule = getRule(parser.previous.type)->infix;
        infixRule(canAssign);
    }

    if (canAssign && match(TK_EQUAL)) { /* Checks if is an invalid assignment */
        compilerError(&parser.previous, "Invalid assignment target.");
        expression();
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
 * Compiles a block of code by parsing declarations and statements until a closing brace (end of
 * block) is found.
 */
static void block() {
    while (!check(TK_RIGHT_BRACE) && !check(TK_EOF)) {
        declaration();
    }

    consume(TK_RIGHT_BRACE, "Expected a '}' after block.");
}

/**
 * Compiles a variable declaration.
 */
static void varDeclaration() {
    uint8_t global = parseVariable("Expected a variable name."); /* Parses a variable name */

    if (match(TK_EQUAL)) {
        expression();
    } else {
        emitByte(OP_NULL);
    }

    consume(TK_SEMICOLON, "Expected a ';' after variable declaration.");
    defineVariable(global);
}

/**
 * Compiles a function body and its parameters.
 */
static void function(FunctionType type) {
    Compiler compiler;
    initCompiler(&compiler, type);
    beginScope();

    /* Compiles the parameter list */
    consume(TK_LEFT_PAREN, "Expected a '(' after function name.");
    if (!check(TK_RIGHT_PAREN)) {
        do {
            current->function->arity++;
            if (current->function->arity > 255)
                compilerError(&parser.current, "Cannot have more than 255 parameters.");
            uint8_t paramConstant = parseVariable("Expected a parameter name.");
            defineVariable(paramConstant);
        } while (match(TK_COMMA));
    }
    consume(TK_RIGHT_PAREN, "Expected a ')' after parameters.");

    /* Compiles the function body */
    consume(TK_LEFT_BRACE, "Expected a '{' before function body.");
    block();

    /* Create the function object */
    ObjFunction *function = endCompiler();
    writeConstant(currentBytecodeChunk(), OBJ_VAL(function), parser.previous.line);
}

/**
 * Compiles a function declaration.
 */
static void funDeclaration() {
    uint8_t global = parseVariable("Expected a function name.");
    markInitialized();
    function(TYPE_FUNCTION);
    defineVariable(global);
}

/**
 * Compiles an expression statement.
 */
static void expressionStatement() {
    expression();
    consume(TK_SEMICOLON, "Expected a ';' after expression.");
    emitByte(OP_POP);
}

/**
 * Compiles a "return" statement.
 */
static void returnStatement() {
    if (current->type == TYPE_SCRIPT) /* Checks if in top level code */
        compilerError(&parser.previous, "Cannot return from top level code.");

    if (match(TK_SEMICOLON)) {
        emitReturn();
    } else {
        expression();
        consume(TK_SEMICOLON, "Expected a ';' after return value.");
        emitByte(OP_RETURN);
    }
}

/**
 * Compiles a statement.
 */
static void statement() {
    if (match(TK_RETURN)) {
        returnStatement();
    } else if (match(TK_LEFT_BRACE)) {
        beginScope();
        block();
        endScope();
    } else {
        expressionStatement();
    }
}

/**
 * Synchronize error recovery.
 */
static void synchronize() {
    parser.panicMode = false;

    while (parser.current.type != TK_EOF) {
        if (parser.previous.type == TK_SEMICOLON) return; /* Sync point (expression end) */

        switch (parser.current.type) { /* Sync point (expression begin) */
            case TK_CLASS:
            case TK_FUNCTION:
            case TK_VAR:
            case TK_FOR:
            case TK_IF:
            case TK_WHILE:
            case TK_RETURN:
                return;
            default:; /* Keep skipping tokens */
        }

        advance();
    }
}

/**
 * Compiles a declaration statement.
 */
static void declaration() {
    if (match(TK_VAR)) {
        varDeclaration();
    } else if (match(TK_FUNCTION)) {
        funDeclaration();
    } else {
        statement();
    }

    if (parser.panicMode) synchronize();
}

/**
 * Compiles a given source code string. The parsing technique used is a Pratt parser, an improved
 * recursive descent parser that associates semantics with tokens instead of grammar rules.
 */
ObjFunction *compile(const char *source) {
    Compiler compiler;
    initCompiler(&compiler, TYPE_SCRIPT);
    initScanner(source);
    initTable(&globalNames);

    /* No panic mode yet */
    parser.hadError = false;
    parser.panicMode = false;

    advance();               /* Get the first token */
    while (!match(TK_EOF)) { /* Main compiler loop */
        declaration();       /* YAPL's grammar entry point */
    }

    freeTable(&globalNames);
    ObjFunction *function = endCompiler();
    return parser.hadError ? NULL : function;
}
