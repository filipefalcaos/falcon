/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * falcon_compiler.c: Falcons's handwritten Parser/Compiler based on Pratt Parsing
 * See Falcon's license in the LICENSE file
 */

#include "falcon_compiler.h"
#include "../lib/falcon_error.h"
#include "../lib/falcon_string.h"
#include <stdlib.h>
#include <string.h>

#ifdef FALCON_DEBUG_LEVEL_01
#include "../vm/falcon_debug.h"
#include <stdio.h>
#endif

/* Compilation flags */
#define FALCON_ERROR_STATE      (-1)
#define FALCON_UNDEFINED_SCOPE  FALCON_ERROR_STATE
#define FALCON_UNRESOLVED_LOCAL FALCON_ERROR_STATE
#define FALCON_GLOBAL_SCOPE     0

/* Parser representation */
typedef struct {
    Token current;  /* The last "lexed" token */
    Token previous; /* The last consumed token */
    bool hadError;  /* Whether a syntax/compile error occurred or not */
    bool panicMode; /* Whether the parser is in error recovery (Panic Mode) or not */
} Parser;

/* Precedence levels, from lowest to highest */
typedef enum {
    PREC_NONE,    /* No precedence */
    PREC_ASSIGN,  /* 1: "=", "-=", "+=", "*=", "/=", "%=" */
    PREC_TERNARY, /* 2: "?:" */
    PREC_OR,      /* 3: "or" */
    PREC_AND,     /* 4: "and" */
    PREC_EQUAL,   /* 5: "==", "!=" */
    PREC_COMPARE, /* 6: "<", ">", "<=", ">=" */
    PREC_TERM,    /* 7: "+", "-" */
    PREC_FACTOR,  /* 8: "*", "/", "%" */
    PREC_UNARY,   /* 9: "not", "-" */
    PREC_POW,     /* 10: "^" */
    PREC_TOP      /* Highest precedence: function calls, lists, and field accesses */
} PrecedenceLevels;

/* Program compiler representation */
typedef struct {
    FalconVM *vm;                /* Falcon's virtual machine instance */
    Parser *parser;              /* Falcon's parser instance */
    Scanner *scanner;            /* Falcon's scanner instance */
    FunctionCompiler *fCompiler; /* The compiler for the currently compiling function */
} FalconCompiler;

/* Function pointer to the parsing functions */
typedef void (*ParseFunction)(FalconCompiler *compiler, bool canAssign);

/* Parsing rules (prefix function, infix function, precedence level) */
typedef struct {
    ParseFunction prefix;
    ParseFunction infix;
    PrecedenceLevels precedence;
} ParseRule;

/**
 * Initializes a given Parser instance as error-free and with no loops.
 */
static void initParser(Parser *parser) {
    parser->hadError = false;
    parser->panicMode = false;
}

/**
 * Presents a syntax/compiler error to the programmer.
 */
void compilerError(FalconCompiler *compiler, Token *token, const char *message) {
    if (compiler->parser->panicMode) return; /* Checks and sets error recovery */
    compiler->parser->panicMode = true;
    falconCompileError(compiler->vm, compiler->scanner, token, message); /* Presents the error */
    compiler->parser->hadError = true;
}

/**
 * Takes the old current token and then loops through the token stream and to get the next token.
 * The loop keeps going reading tokens and reporting the errors, until it hits a non-error one or
 * reach EOF.
 */
static void advance(FalconCompiler *compiler) {
    compiler->parser->previous = compiler->parser->current;

    while (true) {
        compiler->parser->current = falconScanToken(compiler->scanner);
        if (compiler->parser->current.type != TK_ERROR) break;
        compilerError(compiler, &compiler->parser->current, compiler->parser->current.start);
    }
}

/**
 * Reads the next token and validates that the token has an expected type. If not, reports an error.
 */
static void consume(FalconCompiler *compiler, FalconTokens type, const char *message) {
    if (compiler->parser->current.type == type) {
        advance(compiler);
        return;
    }

    compilerError(compiler, &compiler->parser->current, message);
}

/**
 * Checks if the current token if of the given type.
 */
static bool check(Parser *parser, FalconTokens type) {
    return parser->current.type == type;
}

/**
 * Checks if the current token is of the given type. If so, the token is consumed.
 */
static bool match(FalconCompiler *compiler, FalconTokens type) {
    if (!check(compiler->parser, type)) return false;
    advance(compiler);
    return true;
}

/**
 * Returns the compiling function.
 */
static ObjFunction *currentFunction(FunctionCompiler *fCompiler) {
    return fCompiler->function;
}

/**
 * Returns the compiling bytecode chunk.
 */
static BytecodeChunk *currentBytecode(FunctionCompiler *fCompiler) {
    return &currentFunction(fCompiler)->bytecode;
}

/**
 * Appends a single byte to the bytecode chunk.
 */
static void emitByte(FalconCompiler *compiler, uint8_t byte) {
    falconWriteBytecode(compiler->vm, currentBytecode(compiler->fCompiler), byte,
                        compiler->parser->previous.line);
}

/**
 * Appends two bytes (operation code and operand) to the bytecode chunk.
 */
static void emitBytes(FalconCompiler *compiler, uint8_t byte_1, uint8_t byte_2) {
    emitByte(compiler, byte_1);
    emitByte(compiler, byte_2);
}

/**
 * Emits a new 'loop back' instruction which jumps backwards by a given offset.
 */
static void emitLoop(FalconCompiler *compiler, int loopStart) {
    emitByte(compiler, OP_LOOP);
    uint16_t offset = (uint16_t)(currentBytecode(compiler->fCompiler)->count - loopStart + 2);

    if (offset > UINT16_MAX) /* Loop is too long? */
        compilerError(compiler, &compiler->parser->previous, FALCON_LOOP_LIMIT_ERR);

    emitByte(compiler, (uint8_t)((uint16_t)(offset >> 8u) & 0xffu));
    emitByte(compiler, (uint8_t)(offset & 0xffu));
}

/**
 * Emits the bytecode of a given instruction and reserve two bytes for a jump offset.
 */
static int emitJump(FalconCompiler *compiler, uint8_t instruction) {
    emitByte(compiler, instruction);
    emitByte(compiler, 0xff);
    emitByte(compiler, 0xff);
    return currentBytecode(compiler->fCompiler)->count - 2;
}

/**
 * Emits the OP_RETURN bytecode instruction.
 */
static void emitReturn(FalconCompiler *compiler) {
    emitBytes(compiler, OP_NULL_LIT, OP_RETURN);
}

/**
 * Adds a constant to the bytecode chunk constants table.
 */
static uint8_t makeConstant(FalconCompiler *compiler, FalconValue value) {
    int constant = falconAddConstant(compiler->vm, currentBytecode(compiler->fCompiler), value);
    if (constant > UINT8_MAX) {
        compilerError(compiler, &compiler->parser->previous, FALCON_CONST_LIMIT_ERR);
        return 0;
    }

    return (uint8_t) constant;
}

/**
 * Inserts a constant in the constants table of the current bytecode chunk. Before inserting,
 * checks if the constant limit was exceeded.
 */
static void emitConstant(FalconCompiler *compiler, FalconValue value) {
    int constant = falconAddConstant(compiler->vm, currentBytecode(compiler->fCompiler), value);
    if (constant > UINT16_MAX) {
        compilerError(compiler, &compiler->parser->previous, FALCON_CONST_LIMIT_ERR);
    } else {
        falconWriteConstant(compiler->vm, currentBytecode(compiler->fCompiler), (uint16_t) constant,
                            compiler->parser->previous.line);
    }
}

/**
 * Replaces the operand at the given location with the calculated jump offset. This function
 * should be called right before the emission of the next instruction that the jump should
 * land on.
 */
static void patchJump(FalconCompiler *compiler, int offset) {
    uint16_t jump = (uint16_t)(currentBytecode(compiler->fCompiler)->count - offset -
                               2); /* -2 to adjust by offset */

    if (jump > UINT16_MAX) /* Jump is too long? */
        compilerError(compiler, &compiler->parser->previous, FALCON_JUMP_LIMIT_ERR);

    currentBytecode(compiler->fCompiler)->code[offset] = (uint8_t)((uint16_t)(jump >> 8u) & 0xffu);
    currentBytecode(compiler->fCompiler)->code[offset + 1] = (uint8_t)(jump & 0xffu);
}

/**
 * Starts a new program compiler.
 */
static void initCompiler(FalconCompiler *compiler, FalconVM *vm, Parser *parser, Scanner *scanner) {
    compiler->vm = vm;
    compiler->parser = parser;
    compiler->scanner = scanner;
}

/**
 * Starts a new function compilation process.
 */
static void initFunctionCompiler(FalconCompiler *compiler, FunctionCompiler *fCompiler,
                                 FunctionCompiler *enclosing, FunctionType type) {
    fCompiler->enclosing = enclosing;
    fCompiler->function = NULL;
    fCompiler->loop = NULL;
    fCompiler->type = type;
    fCompiler->localCount = 0;
    fCompiler->scopeDepth = FALCON_GLOBAL_SCOPE;
    fCompiler->function = falconFunction(compiler->vm);
    compiler->vm->compiler = compiler->fCompiler = fCompiler;

    Parser *parser = compiler->parser;
    if (type != TYPE_SCRIPT)
        currentFunction(compiler->fCompiler)->name = falconCopyString(
            compiler->vm, parser->previous.start, parser->previous.length); /* Sets function name */

    /* Set stack slot zero for the VM's internal use */
    Local *local = &compiler->fCompiler->locals[compiler->fCompiler->localCount++];
    local->depth = FALCON_GLOBAL_SCOPE;
    local->isCaptured = false;
    local->name.start = "";
    local->name.length = 0;
}

/**
 * Ends the function compilation process.
 */
static ObjFunction *endFunctionCompiler(FalconCompiler *compiler) {
    emitReturn(compiler);
    ObjFunction *function = currentFunction(compiler->fCompiler);

#ifdef FALCON_DEBUG_LEVEL_01
    if (!compiler->parser->hadError) {
        falconDumpBytecode(compiler->vm, currentBytecode(compiler->fCompiler),
                           function->name != NULL ? function->name->chars : FALCON_SCRIPT);
        printf("\n");
    }
#endif

    compiler->vm->compiler = compiler->fCompiler = compiler->fCompiler->enclosing;
    return function;
}

/**
 * Begins a new scope by incrementing the current scope depth.
 */
static void beginScope(FunctionCompiler *fCompiler) { fCompiler->scopeDepth++; }

/**
 * Ends an existing scope by decrementing the current scope depth and popping all local variables
 * declared in the scope.
 */
static void endScope(FalconCompiler *compiler) {
    FunctionCompiler *fCompiler = compiler->fCompiler;
    fCompiler->scopeDepth--;

    /* Closes locals and upvalues in the scope */
    while (fCompiler->localCount > 0 &&
           fCompiler->locals[fCompiler->localCount - 1].depth > fCompiler->scopeDepth) {
        if (fCompiler->locals[fCompiler->localCount - 1].isCaptured) {
            emitByte(compiler, OP_CLOSE_UPVALUE);
        } else {
            emitByte(compiler, OP_POP);
        }

        fCompiler->localCount--;
    }
}

/* Forward parser's declarations, since the grammar is recursive */
static void expression(FalconCompiler *compiler);
static void statement(FalconCompiler *compiler);
static void declaration(FalconCompiler *compiler);
static ParseRule *getParseRule(FalconTokens type);
static void parsePrecedence(FalconCompiler *compiler, PrecedenceLevels precedence);

/**
 * Checks if an identifier constant was already defined. If so, return its index. If not, set the
 * identifier in the global names table.
 */
static uint8_t identifierConstant(FalconCompiler *compiler, Token *name) {
    return makeConstant(compiler,
                        FALCON_OBJ_VAL(falconCopyString(compiler->vm, name->start, name->length)));
}

/**
 * Checks if two identifiers match.
 */
static bool identifiersEqual(Token *a, Token *b) {
    if (a->length != b->length) return false;
    return memcmp(a->start, b->start, (size_t) a->length) == 0;
}

/**
 * Resolves a local variable by looping through the list of locals that are currently in the scope.
 * If one has the same name as the identifier token, the variable is resolved.
 */
static int resolveLocal(FalconCompiler *compiler, FunctionCompiler *fCompiler, Token *name) {
    for (int i = fCompiler->localCount - 1; i >= 0; i--) {
        Local *local = &fCompiler->locals[i];
        if (identifiersEqual(name, &local->name)) { /* Checks if identifier matches */
            if (local->depth == FALCON_UNDEFINED_SCOPE)
                compilerError(compiler, &compiler->parser->previous, FALCON_RED_INIT_ERR);
            return i;
        }
    }

    return FALCON_UNRESOLVED_LOCAL;
}

/**
 * Adds a new upvalue to the upvalue list and returns the index of the created upvalue.
 */
static int addUpvalue(FalconCompiler *compiler, FunctionCompiler *fCompiler, uint8_t index,
                      bool isLocal) {
    ObjFunction *function = currentFunction(fCompiler);
    int upvalueCount = function->upvalueCount;

    /* Checks if the upvalue is already in the upvalue list */
    for (int i = 0; i < upvalueCount; i++) {
        Upvalue *upvalue = &fCompiler->upvalues[i];
        if (upvalue->index == index && upvalue->isLocal == isLocal) return i;
    }

    if (upvalueCount == FALCON_MAX_BYTE) { /* Exceeds the limit os upvalues? */
        compilerError(compiler, &compiler->parser->previous, FALCON_CLOSURE_LIMIT_ERR);
        return FALCON_UNRESOLVED_LOCAL;
    }

    /* Adds the new upvalue */
    fCompiler->upvalues[upvalueCount].isLocal = isLocal;
    fCompiler->upvalues[upvalueCount].index = index;
    return function->upvalueCount++;
}

/**
 * Resolves an upvalue by looking for a local variable declared in any of the surrounding scopes. If
 * found, an upvalue index for that variable is returned.
 */
static int resolveUpvalue(FalconCompiler *compiler, FunctionCompiler *fCompiler,
                          Token *name) {
    if (fCompiler->enclosing == NULL) /* Global variable? */
        return FALCON_UNRESOLVED_LOCAL;

    /* Looks for a local variable in the enclosing scope */
    int local = resolveLocal(compiler, fCompiler->enclosing, name);
    if (local != FALCON_UNRESOLVED_LOCAL) {
        fCompiler->enclosing->locals[local].isCaptured = true;
        return addUpvalue(compiler, fCompiler, (uint8_t) local, true);
    }

    /* Looks for an upvalue in the enclosing scope */
    int upvalue = resolveUpvalue(compiler, fCompiler->enclosing, name);
    if (upvalue != FALCON_UNRESOLVED_LOCAL)
        return addUpvalue(compiler, fCompiler, (uint8_t) upvalue, false);

    return FALCON_UNRESOLVED_LOCAL;
}

/**
 * Adds a local variable to the list of variables in a scope depth.
 */
static void addLocal(FalconCompiler *compiler, Token name) {
    FunctionCompiler *fCompiler = compiler->fCompiler;
    if (fCompiler->localCount == FALCON_MAX_BYTE) {
        compilerError(compiler, &compiler->parser->previous, FALCON_VAR_LIMIT_ERR);
        return;
    }

    Local *local = &fCompiler->locals[fCompiler->localCount++];
    local->name = name;
    local->depth = FALCON_UNDEFINED_SCOPE;
    local->isCaptured = false;
}

/**
 * Records the existence of a variable declaration.
 */
static void declareVariable(FalconCompiler *compiler) {
    FunctionCompiler *fCompiler = compiler->fCompiler;
    if (fCompiler->scopeDepth == FALCON_GLOBAL_SCOPE) return; /* Globals are late bound, exit */
    Token *name = &compiler->parser->previous;

    /* Verifies if local variable was previously declared */
    for (int i = fCompiler->localCount - 1; i >= 0; i--) {
        Local *local = &fCompiler->locals[i];
        if (local->depth != FALCON_UNDEFINED_SCOPE && local->depth < fCompiler->scopeDepth) break;
        if (identifiersEqual(name, &local->name)) /* Checks if already declared */
            compilerError(compiler, &compiler->parser->previous, FALCON_VAR_REDECL_ERR);
    }

    addLocal(compiler, *name);
}

/**
 * Parses a variable by consuming an identifier token and then adds the the token lexeme to the
 * constants table.
 */
static uint8_t parseVariable(FalconCompiler *compiler, const char *errorMessage) {
    consume(compiler, TK_IDENTIFIER, errorMessage);
    declareVariable(compiler); /* Declares the variables */

    if (compiler->fCompiler->scopeDepth > FALCON_GLOBAL_SCOPE)
        return 0; /* Locals are not looked up by name, exit */

    return identifierConstant(compiler, &compiler->parser->previous);
}

/**
 * Marks a local variable as initialized and available for use.
 */
static void markInitialized(FunctionCompiler *fCompiler) {
    if (fCompiler->scopeDepth == FALCON_GLOBAL_SCOPE) return;
    fCompiler->locals[fCompiler->localCount - 1].depth = fCompiler->scopeDepth;
}

/**
 * Handles a variable declaration by emitting the bytecode to perform a global variable declaration.
 */
static void defineVariable(FalconCompiler *compiler, uint8_t global) {
    if (compiler->fCompiler->scopeDepth > FALCON_GLOBAL_SCOPE) {
        markInitialized(compiler->fCompiler); /* Mark as initialized */
        return;                               /* Only globals are defined at runtime */
    }

    emitBytes(compiler, OP_DEF_GLOBAL, global);
}

/**
 * Compiles the list of arguments in a function call.
 */
static uint8_t argumentList(FalconCompiler *compiler) {
    uint8_t argCount = 0;
    if (!check(compiler->parser, TK_RIGHT_PAREN)) {
        do {
            expression(compiler);
            if (argCount == UINT8_MAX)
                compilerError(compiler, &compiler->parser->previous, FALCON_ARGS_LIMIT_ERR);
            argCount++;
        } while (match(compiler, TK_COMMA));
    }

    return argCount;
}

/**
 * Emits the instructions for a given compound assignment, which provide a shorter syntax for
 * assigning the result of an arithmetic operator.
 */
static void compoundAssignment(FalconCompiler *compiler, uint8_t getOpcode, uint8_t setOpcode,
                               int arg, uint8_t opcode) {
    emitBytes(compiler, getOpcode, (uint8_t) arg);
    expression(compiler);
    emitByte(compiler, opcode);
    emitBytes(compiler, setOpcode, (uint8_t) arg);
}

/**
 * Gets the index of a variable in the constants table and emits the the bytecode to perform the
 * load of the global/local variable.
 */
static void namedVariable(FalconCompiler *compiler, Token name, bool canAssign) {
    uint8_t getOpcode, setOpcode;
    int arg = resolveLocal(compiler, compiler->fCompiler, &name);

    /* Finds the current scope */
    if (arg != FALCON_UNRESOLVED_LOCAL) { /* Local variable? */
        getOpcode = OP_GET_LOCAL;
        setOpcode = OP_SET_LOCAL;
    } else if ((arg = resolveUpvalue(compiler, compiler->fCompiler, &name)) != -1) { /* Upvalue? */
        getOpcode = OP_GET_UPVALUE;
        setOpcode = OP_SET_UPVALUE;
    } else { /* Global variable */
        arg = identifierConstant(compiler, &name);
        getOpcode = OP_GET_GLOBAL;
        setOpcode = OP_SET_GLOBAL;
    }

    /* Compiles variable assignments or access */
    if (canAssign && match(compiler, TK_EQUAL)) { /* a = ... */
        expression(compiler);
        emitBytes(compiler, setOpcode, (uint8_t) arg);
    } else if (canAssign && match(compiler, TK_MINUS_EQUAL)) { /* a -= ... */
        compoundAssignment(compiler, getOpcode, setOpcode, arg, OP_SUBTRACT);
    } else if (canAssign && match(compiler, TK_PLUS_EQUAL)) { /* a += ... */
        compoundAssignment(compiler, getOpcode, setOpcode, arg, OP_ADD);
    } else if (canAssign && match(compiler, TK_DIV_EQUAL)) { /* a /= ... */
        compoundAssignment(compiler, getOpcode, setOpcode, arg, OP_DIVIDE);
    } else if (canAssign && match(compiler, TK_MOD_EQUAL)) { /* a %= ... */
        compoundAssignment(compiler, getOpcode, setOpcode, arg, OP_MOD);
    } else if (canAssign && match(compiler, TK_MULTIPLY_EQUAL)) { /* a *= ... */
        compoundAssignment(compiler, getOpcode, setOpcode, arg, OP_MULTIPLY);
    } else if (canAssign && match(compiler, TK_POW_EQUAL)) { /* a ^= ... */
        compoundAssignment(compiler, getOpcode, setOpcode, arg, OP_POW);
    } else { /* Access variable */
        emitBytes(compiler, getOpcode, (uint8_t) arg);
    }
}

/* Common interface to all parsing rules */
#define PARSE_RULE(name) static void name(FalconCompiler *compiler, bool canAssign)

/**
 * Handles the "and" logical operator with short-circuit.
 */
PARSE_RULE(and_) {
    (void) canAssign; /* Unused */
    int jump = emitJump(compiler, OP_AND);
    parsePrecedence(compiler, PREC_AND);
    patchJump(compiler, jump);
}

/**
 * Handles the "or" logical operator with short-circuit.
 */
PARSE_RULE(or_) {
    (void) canAssign; /* Unused */
    int jump = emitJump(compiler, OP_OR);
    parsePrecedence(compiler, PREC_OR);
    patchJump(compiler, jump);
}

/**
 * Handles a binary (infix) expression, by compiling the right operand of the expression (the left
 * one was already compiled). Then, emits the bytecode instruction that performs the binary
 * operation.
 */
PARSE_RULE(binary) {
    (void) canAssign; /* Unused */
    FalconTokens operatorType = compiler->parser->previous.type;
    ParseRule *rule = getParseRule(operatorType);    /* Gets the current rule */
    parsePrecedence(compiler, rule->precedence + 1); /* Compiles with the correct precedence */

    /* Emits the operator instruction */
    switch (operatorType) {
        case TK_NOT_EQUAL:
            emitBytes(compiler, OP_EQUAL, OP_NOT);
            break;
        case TK_EQUAL_EQUAL:
            emitByte(compiler, OP_EQUAL);
            break;
        case TK_GREATER:
            emitByte(compiler, OP_GREATER);
            break;
        case TK_GREATER_EQUAL:
            emitBytes(compiler, OP_LESS, OP_NOT);
            break;
        case TK_LESS:
            emitByte(compiler, OP_LESS);
            break;
        case TK_LESS_EQUAL:
            emitBytes(compiler, OP_GREATER, OP_NOT);
            break;
        case TK_PLUS:
            emitByte(compiler, OP_ADD);
            break;
        case TK_MINUS:
            emitByte(compiler, OP_SUBTRACT);
            break;
        case TK_DIV:
            emitByte(compiler, OP_DIVIDE);
            break;
        case TK_MOD:
            emitByte(compiler, OP_MOD);
            break;
        case TK_MULTIPLY:
            emitByte(compiler, OP_MULTIPLY);
            break;
        default:
            return; /* Unreachable */
    }
}

/**
 * Handles a function call expression by parsing its arguments list and emitting the instruction to
 * proceed with the execution of the function.
 */
PARSE_RULE(call) {
    (void) canAssign; /* Unused */
    uint8_t argCount = argumentList(compiler);
    consume(compiler, TK_RIGHT_PAREN, FALCON_CALL_LIST_PAREN_ERR);
    emitBytes(compiler, OP_CALL, argCount);
}

/**
 * Handles the opening parenthesis by compiling the expression between the parentheses, and then
 * parsing the closing parenthesis.
 */
PARSE_RULE(grouping) {
    (void) canAssign; /* Unused */
    expression(compiler);
    consume(compiler, TK_RIGHT_PAREN, FALCON_GRP_EXPR_ERR);
}

/**
 * Handles a list literal expression by creating a new Falcon native List and compiling each of
 * its elements.
 */
PARSE_RULE(list) {
    (void) canAssign; /* Unused */
    emitBytes(compiler, OP_LIST, FALCON_MIN_LIST); /* Creates a new list with default size */

    if (!check(compiler->parser, TK_RIGHT_PAREN)) {
        do {
            expression(compiler);
            emitByte(compiler, OP_PUSH_LIST);
        } while (match(compiler, TK_COMMA));
    }

    consume(compiler, TK_RIGHT_BRACKET, FALCON_LIST_BRACKET_ERR);
}

/**
 * Handles a literal (booleans or null) expression by outputting the proper instruction.
 */
PARSE_RULE(literal) {
    (void) canAssign; /* Unused */
    switch (compiler->parser->previous.type) {
        case TK_FALSE:
            emitByte(compiler, OP_FALSE_LIT);
            break;
        case TK_NULL:
            emitByte(compiler, OP_NULL_LIT);
            break;
        case TK_TRUE:
            emitByte(compiler, OP_TRUE_LIT);
            break;
        default:
            return; /* Unreachable */
    }
}

/**
 * Handles a numeric expression by converting a string to a double number and then generates the
 * code to load that value by calling "emitConstant".
 */
PARSE_RULE(number) {
    (void) canAssign; /* Unused */
    double value = strtod(compiler->parser->previous.start, NULL);
    emitConstant(compiler, FALCON_NUM_VAL(value));
}

/**
 * Handles a exponentiation expression.
 */
PARSE_RULE(pow_) {
    (void) canAssign;                    /* Unused */
    parsePrecedence(compiler, PREC_POW); /* Compiles the operand */
    emitByte(compiler, OP_POW);
}

/**
 * Handles a string expression by creating a string object, wrapping it in a Value, and then
 * adding it to the constants table.
 */
PARSE_RULE(string) {
    (void) canAssign; /* Unused */
    Parser *parser = compiler->parser;
    emitConstant(compiler, FALCON_OBJ_VAL(falconCopyString(compiler->vm, parser->previous.start + 1,
                                                           parser->previous.length - 2)));
}

/**
 * Handles the subscript (list indexing) operator expression by compiling the index expression.
 */
PARSE_RULE(subscript) {
    (void) canAssign; /* Unused */
}

/**
 * Handles the ternary "?:" conditional operator expression.
 */
PARSE_RULE(ternary) {
    (void) canAssign;                                  /* Unused */
    int ifJump = emitJump(compiler, OP_JUMP_IF_FALSE); /* Jumps if the condition is false */
    emitByte(compiler, OP_POP);                        /* Pops the condition result */
    parsePrecedence(compiler, PREC_TERNARY);           /* Compiles the first branch */
    consume(compiler, TK_COLON, FALCON_TERNARY_EXPR_ERR);

    int elseJump = emitJump(compiler, OP_JUMP); /* Jumps the second branch if first was taken */
    patchJump(compiler, ifJump);                /* Patches the jump over the first branch */
    emitByte(compiler, OP_POP);                 /* Pops the condition result */
    parsePrecedence(compiler, PREC_ASSIGN);     /* Compiles the second branch */
    patchJump(compiler, elseJump);              /* Patches the jump over the second branch */
}

/**
 * Handles a unary expression by compiling the operand and then emitting the bytecode to perform
 * the unary operation itself.
 */
PARSE_RULE(unary) {
    (void) canAssign; /* Unused */
    FalconTokens operatorType = compiler->parser->previous.type;
    parsePrecedence(compiler, PREC_UNARY); /* Compiles the operand */

    switch (operatorType) {
        case TK_MINUS:
            emitByte(compiler, OP_NEGATE);
            break;
        case TK_NOT:
            emitByte(compiler, OP_NOT);
            break;
        default:
            return;
    }
}

/**
 * Handles a variable access.
 */
PARSE_RULE(variable) { namedVariable(compiler, compiler->parser->previous, canAssign); }

#undef PARSE_RULE

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
    RULE(grouping, call, PREC_TOP),    /* TK_LEFT_PAREN */
    EMPTY_RULE,                        /* TK_RIGHT_PAREN */
    EMPTY_RULE,                        /* TK_LEFT_BRACE */
    EMPTY_RULE,                        /* TK_RIGHT_BRACE */
    RULE(list, subscript, PREC_TOP),   /* TK_LEFT_BRACKET */
    EMPTY_RULE,                        /* TK_RIGHT_BRACKET */
    EMPTY_RULE,                        /* TK_COMMA */
    EMPTY_RULE,                        /* TK_DOT */
    EMPTY_RULE,                        /* TK_COLON */
    EMPTY_RULE,                        /* TK_SEMICOLON */
    EMPTY_RULE,                        /* TK_ARROW */
    RULE(unary, binary, PREC_TERM),    /* TK_MINUS */
    EMPTY_RULE,                        /* TK_MINUS_EQUAL */
    INFIX_RULE(binary, PREC_TERM),     /* TK_PLUS */
    EMPTY_RULE,                        /* TK_PLUS_EQUAL */
    INFIX_RULE(binary, PREC_FACTOR),   /* TK_DIV */
    EMPTY_RULE,                        /* TK_DIV_EQUAL */
    INFIX_RULE(binary, PREC_FACTOR),   /* TK_MOD */
    EMPTY_RULE,                        /* TK_MOD_EQUAL */
    INFIX_RULE(binary, PREC_FACTOR),   /* TK_MULTIPLY */
    EMPTY_RULE,                        /* TK_MULTIPLY_EQUAL */
    INFIX_RULE(pow_, PREC_POW),        /* TK_POW */
    EMPTY_RULE,                        /* TK_POW_EQUAL */
    PREFIX_RULE(unary),                /* TK_NOT */
    INFIX_RULE(binary, PREC_EQUAL),    /* TK_NOT_EQUAL */
    EMPTY_RULE,                        /* TK_EQUAL */
    INFIX_RULE(binary, PREC_EQUAL),    /* TK_EQUAL_EQUAL */
    INFIX_RULE(binary, PREC_COMPARE),  /* TK_GREATER */
    INFIX_RULE(binary, PREC_COMPARE),  /* TK_GREATER_EQUAL */
    INFIX_RULE(binary, PREC_COMPARE),  /* TK_LESS */
    INFIX_RULE(binary, PREC_COMPARE),  /* TK_LESS_EQUAL */
    INFIX_RULE(and_, PREC_AND),        /* TK_AND */
    INFIX_RULE(or_, PREC_OR),          /* TK_OR */
    INFIX_RULE(ternary, PREC_TERNARY), /* TK_TERNARY */
    PREFIX_RULE(variable),             /* TK_IDENTIFIER */
    PREFIX_RULE(string),               /* TK_STRING */
    PREFIX_RULE(number),               /* TK_NUMBER */
    EMPTY_RULE,                        /* TK_BREAK */
    EMPTY_RULE,                        /* TK_CLASS */
    EMPTY_RULE,                        /* TK_ELSE */
    PREFIX_RULE(literal),              /* TK_FALSE */
    EMPTY_RULE,                        /* TK_FOR */
    EMPTY_RULE,                        /* TK_FUNCTION */
    EMPTY_RULE,                        /* TK_IF */
    EMPTY_RULE,                        /* TK_NEXT */
    PREFIX_RULE(literal),              /* TK_NULL */
    EMPTY_RULE,                        /* TK_RETURN */
    EMPTY_RULE,                        /* TK_SUPER */
    EMPTY_RULE,                        /* TK_SWITCH */
    EMPTY_RULE,                        /* TK_THIS */
    PREFIX_RULE(literal),              /* TK_TRUE */
    EMPTY_RULE,                        /* TK_VAR */
    EMPTY_RULE,                        /* TK_WHEN */
    EMPTY_RULE,                        /* TK_WHILE */
    EMPTY_RULE,                        /* TK_ERROR */
    EMPTY_RULE                         /* TK_EOF */
};

#undef EMPTY_RULE
#undef PREFIX_RULE
#undef INFIX_RULE
#undef RULE

/**
 * Parses any expression of a given precedence level or higher. Reads the next token and looks up
 * the corresponding ParseRule. If there is no prefix parser, then the token must be a syntax error.
 */
static void parsePrecedence(FalconCompiler *compiler, PrecedenceLevels precedence) {
    advance(compiler);
    Parser *parser = compiler->parser;
    ParseFunction prefixRule = getParseRule(parser->previous.type)->prefix;

    if (prefixRule == NULL) { /* Checks if is a parsing error */
        compilerError(compiler, &parser->previous, FALCON_EXPR_ERR);
        return;
    }

    /* Checks if the left side is assignable */
    bool canAssign = precedence <= PREC_TERNARY;
    prefixRule(compiler, canAssign);

    /* Looks for an infix parser for the next token */
    while (precedence <= getParseRule(parser->current.type)->precedence) {
        advance(compiler);
        ParseFunction infixRule = getParseRule(parser->previous.type)->infix;
        infixRule(compiler, canAssign);
    }

    if (canAssign && match(compiler, TK_EQUAL)) { /* Checks if is an invalid assignment */
        compilerError(compiler, &parser->previous, FALCON_INV_ASSG_ERR);
    }
}

/**
 * Returns the rule at a given index.
 */
static ParseRule *getParseRule(FalconTokens type) { return &rules[type]; }

/**
 * Compiles an expression by parsing the lowest precedence level, which subsumes all of the higher
 * precedence expressions too.
 */
static void expression(FalconCompiler *compiler) { parsePrecedence(compiler, PREC_ASSIGN); }

/**
 * Compiles a block of code by parsing declarations and statements until a closing brace (end of
 * block) is found.
 */
static void block(FalconCompiler *compiler) {
    while (!check(compiler->parser, TK_RIGHT_BRACE) &&
           !check(compiler->parser, TK_EOF)) {
        declaration(compiler);
    }

    consume(compiler, TK_RIGHT_BRACE, FALCON_BLOCK_BRACE_ERR);
}

/**
 * Compiles the declaration of a single variable.
 */
static void singleVarDeclaration(FalconCompiler *compiler) {
    uint8_t global = parseVariable(compiler, FALCON_VAR_NAME_ERR); /* Parses a variable name */

    if (match(compiler, TK_EQUAL)) {
        expression(compiler); /* Compiles the variable initializer */
    } else {
        emitByte(compiler, OP_NULL_LIT); /* Default variable value is "null" */
    }

    defineVariable(compiler, global); /* Emits the declaration bytecode */
}

/**
 * Compiles a variable declaration list.
 */
static void varDeclaration(FalconCompiler *compiler) {
    Parser *parser = compiler->parser;
    if (!check(parser, TK_SEMICOLON)) {
        do {
            singleVarDeclaration(compiler); /* Compiles the declaration */
        } while (match(compiler, TK_COMMA));
    }
}

/**
 * Compiles a function body and its parameters.
 */
static void function(FalconCompiler *compiler, FunctionType type) {
    Parser *parser = compiler->parser;
    FunctionCompiler fCompiler;
    initFunctionCompiler(compiler, &fCompiler, compiler->fCompiler, type);
    beginScope(compiler->fCompiler);

    /* Compiles the parameter list */
    consume(compiler, TK_LEFT_PAREN, FALCON_FUNC_NAME_PAREN_ERR);
    if (!check(parser, TK_RIGHT_PAREN)) {
        do {
            ObjFunction *current = currentFunction(compiler->fCompiler);
            current->arity++;

            if (current->arity > UINT8_MAX) /* Exceeds the limit os parameters? */
                compilerError(compiler, &parser->current, FALCON_PARAMS_LIMIT_ERR);

            uint8_t paramConstant = parseVariable(compiler, FALCON_PARAM_NAME_ERR);
            defineVariable(compiler, paramConstant);
        } while (match(compiler, TK_COMMA));
    }
    consume(compiler, TK_RIGHT_PAREN, FALCON_FUNC_LIST_PAREN_ERR);

    /* Compiles the function body */
    consume(compiler, TK_LEFT_BRACE, FALCON_FUNC_BODY_BRACE_ERR);
    block(compiler);

    /* Create the function object */
    ObjFunction *function = endFunctionCompiler(compiler);
    emitBytes(compiler, OP_CLOSURE, makeConstant(compiler, FALCON_OBJ_VAL(function)));

    /* Emits the captured upvalues */
    for (int i = 0; i < function->upvalueCount; i++) {
        emitByte(compiler, (uint8_t)(fCompiler.upvalues[i].isLocal ? true : false));
        emitByte(compiler, fCompiler.upvalues[i].index);
    }
}

/**
 * Compiles a function declaration.
 */
static void funDeclaration(FalconCompiler *compiler) {
    uint8_t func = parseVariable(compiler, FALCON_FUNC_NAME_ERR);
    markInitialized(compiler->fCompiler);
    function(compiler, TYPE_FUNCTION);
    defineVariable(compiler, func);
}

/**
 * Compiles an expression statement.
 */
static void expressionStatement(FalconCompiler *compiler) {
    expression(compiler);
    consume(compiler, TK_SEMICOLON, FALCON_EXPR_STMT_ERR);
    bool retRepl = compiler->vm->isREPL && compiler->fCompiler->scopeDepth == FALCON_GLOBAL_SCOPE;
    emitByte(compiler, retRepl ? OP_POP_EXPR : OP_POP);
}

/**
 * Compiles an "if" conditional statement.
 */
static void ifStatement(FalconCompiler *compiler) {
    expression(compiler); /* Compiles condition */
    consume(compiler, TK_LEFT_BRACE, FALCON_IF_STMT_ERR);

    int thenJump = emitJump(compiler, OP_JUMP_IF_FALSE);
    emitByte(compiler, OP_POP);

    /* Compiles the "if" block */
    beginScope(compiler->fCompiler);
    block(compiler);
    endScope(compiler);

    int elseJump = emitJump(compiler, OP_JUMP);
    patchJump(compiler, thenJump);
    emitByte(compiler, OP_POP);

    /* Compiles the "else" block */
    if (match(compiler, TK_ELSE)) {
        if (match(compiler, TK_IF)) {
            ifStatement(compiler); /* "else if ..." form */
        } else if (match(compiler, TK_LEFT_BRACE)) {
            beginScope(compiler->fCompiler);
            block(compiler); /* Compiles the "else" branch */
            endScope(compiler);
        }
    }

    patchJump(compiler, elseJump);
}

/**
 * Compiles a "switch" conditional statement.
 */
static void switchStatement(FalconCompiler *compiler) {
    Parser *parser = compiler->parser;

/* Possible switch states */
#define FALCON_BEF_CASES (0)
#define FALCON_BEF_ELSE  (1)
#define FALCON_AFT_ELSE  (2)

    int switchState = FALCON_BEF_CASES;
    int caseEnds[FALCON_MAX_BYTE];
    int caseCount = 0;
    int previousCaseSkip = -1;

    expression(compiler); /* Compiles expression to switch on */
    consume(compiler, TK_LEFT_BRACE, FALCON_SWITCH_STMT_ERR);

    while (!match(compiler, TK_RIGHT_BRACE) && !check(parser, TK_EOF)) {
        if (match(compiler, TK_WHEN) || match(compiler, TK_ELSE)) {
            FalconTokens caseType = parser->previous.type;

            if (switchState == FALCON_AFT_ELSE) { /* Already compiled the else case? */
                compilerError(compiler, &parser->previous, FALCON_ELSE_END_ERR);
            } else if (switchState == FALCON_BEF_ELSE) { /* Else case not compiled yet? */
                caseEnds[caseCount++] = emitJump(compiler, OP_JUMP); /* Jumps the other cases */
                patchJump(compiler, previousCaseSkip);               /* Patches the jump */
                emitByte(compiler, OP_POP);
            }

            if (caseType == TK_WHEN) {
                switchState = FALCON_BEF_ELSE;

                /* Checks if the case is equal to the switch value */
                emitByte(compiler, OP_DUP); /* "==" pops its operand, so duplicate before */
                expression(compiler);
                consume(compiler, TK_ARROW, FALCON_ARR_CASE_ERR);
                emitByte(compiler, OP_EQUAL);
                previousCaseSkip = emitJump(compiler, OP_JUMP_IF_FALSE);

                emitByte(compiler, OP_POP); /* Pops the comparison result */
            } else {
                switchState = FALCON_AFT_ELSE;
                consume(compiler, TK_ARROW, FALCON_ARR_ELSE_ERR);
                previousCaseSkip = -1;
            }
        } else {
            if (switchState == FALCON_BEF_CASES) /* Statement outside a case? */
                compilerError(compiler, &parser->previous, FALCON_STMT_SWITCH_ERR);
            statement(compiler); /* Statement is inside a case */
        }
    }

    /* If no else case, patch its condition jump */
    if (switchState == FALCON_BEF_ELSE) {
        patchJump(compiler, previousCaseSkip);
        emitByte(compiler, OP_POP);
    }

    /* Patch all the case jumps to the end */
    for (int i = 0; i < caseCount; i++) {
        patchJump(compiler, caseEnds[i]);
    }

    emitByte(compiler, OP_POP); /* Pops the switch value */

#undef FALCON_BEF_CASES
#undef FALCON_BEF_ELSE
#undef FALCON_AFT_ELSE
}

/* Starts the compilation of a new loop by setting the entry point to the current bytecode chunk
 * instruction */
#define START_LOOP(fCompiler)                       \
    Loop loop;                                      \
    loop.enclosing = (fCompiler)->loop;             \
    loop.entry = currentBytecode(fCompiler)->count; \
    loop.scopeDepth = (fCompiler)->scopeDepth;      \
    (fCompiler)->loop = &loop

/* Compiles the body of a loop and sets its index */
#define LOOP_BODY(compiler)                                                            \
    compiler->fCompiler->loop->body = (compiler)->fCompiler->function->bytecode.count; \
    block(compiler)

/**
 * Gets the number of arguments for a instruction at the given program counter.
 */
int instructionArgs(const BytecodeChunk *bytecode, int pc) {
    switch (bytecode->code[pc]) {
        case OP_FALSE_LIT:
        case OP_TRUE_LIT:
        case OP_NULL_LIT:
        case OP_PUSH_LIST:
        case OP_NOT:
        case OP_EQUAL:
        case OP_GREATER:
        case OP_LESS:
        case OP_ADD:
        case OP_SUBTRACT:
        case OP_NEGATE:
        case OP_DIVIDE:
        case OP_MOD:
        case OP_MULTIPLY:
        case OP_POW:
        case OP_CLOSE_UPVALUE:
        case OP_RETURN:
        case OP_DUP:
        case OP_POP:
        case OP_POP_EXPR:
        case OP_TEMP:
            return 0; /* Instructions with no arguments */

        case OP_LIST:
        case OP_DEF_GLOBAL:
        case OP_GET_GLOBAL:
        case OP_SET_GLOBAL:
        case OP_GET_UPVALUE:
        case OP_SET_UPVALUE:
        case OP_GET_LOCAL:
        case OP_SET_LOCAL:
        case OP_CALL:
            return 1; /* Instructions with single byte as argument */

        case OP_CONSTANT:
        case OP_AND:
        case OP_OR:
        case OP_JUMP:
        case OP_JUMP_IF_FALSE:
        case OP_LOOP:
            return 2; /* Instructions with 2 bytes as arguments */

        case OP_CLOSURE: {
            int index = bytecode->code[pc + 1];
            ObjFunction *function = FALCON_AS_FUNCTION(bytecode->constants.values[index]);
            return 1 + function->upvalueCount * 2; /* Function: 1 byte; Upvalues: 2 bytes each */
        }

        default:
            return 0;
    }
}

/**
 * Ends the current innermost loop on the compiler. If any temporary "OP_TEMP" instruction is in
 * the bytecode, replaces it with the correct "OP_JUMP" instruction and patches the jump to the end
 * of the loop.
 */
static void endLoop(FalconCompiler *compiler) {
    FunctionCompiler *fCompiler = compiler->fCompiler;
    BytecodeChunk *bytecode = &fCompiler->function->bytecode;
    int index = fCompiler->loop->body;

    while (index < bytecode->count) {
        if (bytecode->code[index] == OP_TEMP) { /* Is a temporary for a "break"? */
            bytecode->code[index] = OP_JUMP;    /* Set the correct "OP_JUMP" instruction */
            patchJump(compiler, index + 1);     /* Patch the jump to the end of the loop */
            index += 3;
        } else { /* Jumps the instruction and its arguments */
            index += 1 + instructionArgs(bytecode, index); /* +1 byte - instruction */
        }
    }

    fCompiler->loop = fCompiler->loop->enclosing;
}

/**
 * Compiles a "while" loop statement.
 */
static void whileStatement(FalconCompiler *compiler) {
    FunctionCompiler *fCompiler = compiler->fCompiler;

    START_LOOP(fCompiler); /* Starts a bew loop */
    expression(compiler);  /* Compiles the loop condition */
    consume(compiler, TK_LEFT_BRACE, FALCON_WHILE_STMT_ERR);
    int exitJump = emitJump(compiler, OP_JUMP_IF_FALSE);
    emitByte(compiler, OP_POP);

    /* Compiles the "while" block */
    beginScope(fCompiler);
    LOOP_BODY(compiler);
    endScope(compiler);

    /* Emits the loop and patches the next jump */
    emitLoop(compiler, fCompiler->loop->entry);
    patchJump(compiler, exitJump);
    emitByte(compiler, OP_POP);
    endLoop(compiler); /* Ends the loop */
}

/**
 * Compiles a "for" loop statement.
 */
static void forStatement(FalconCompiler *compiler) {
    FunctionCompiler *fCompiler = compiler->fCompiler;
    beginScope(compiler->fCompiler); /* Begins the loop scope */

    /* Compiles the initializer clause */
    if (match(compiler, TK_COMMA)) { /* Empty initializer? */
        compilerError(compiler, &compiler->parser->previous, FALCON_FOR_STMT_INIT_ERR);
    } else {
        singleVarDeclaration(compiler); /* Variable declaration initializer */
        consume(compiler, TK_COMMA, FALCON_FOR_STMT_CM1_ERR);
    }

    START_LOOP(fCompiler); /* Starts a bew loop */

    /* Compiles the conditional clause */
    expression(compiler);
    consume(compiler, TK_COMMA, FALCON_FOR_STMT_CM2_ERR);
    int exitJump = emitJump(compiler, OP_JUMP_IF_FALSE);
    emitByte(compiler, OP_POP); /* Pops condition */

    /* Compiles the increment clause */
    int bodyJump = emitJump(compiler, OP_JUMP);
    int incrementStart = currentBytecode(fCompiler)->count;
    expression(compiler);
    emitByte(compiler, OP_POP); /* Pops increment */
    consume(compiler, TK_LEFT_BRACE, FALCON_FOR_STMT_BRC_ERR);
    emitLoop(compiler, fCompiler->loop->entry);
    fCompiler->loop->entry = incrementStart;
    patchJump(compiler, bodyJump);

    LOOP_BODY(compiler); /* Compiles the "for" block */
    emitLoop(compiler, fCompiler->loop->entry);
    if (exitJump != -1) {
        patchJump(compiler, exitJump);
        emitByte(compiler, OP_POP); /* Pops condition */
    }

    endScope(compiler); /* Ends the loop scope */
    endLoop(compiler);  /* Ends the loop */
}

#undef START_LOOP
#undef LOOP_BODY

/* Checks if the current scope is outside of a loop body */
#define CHECK_LOOP_ERROR(fCompiler, error)                               \
    do {                                                                 \
        if ((fCompiler)->loop == NULL) /* Is outside of a loop body? */  \
            compilerError(compiler, &compiler->parser->previous, error); \
    } while (false)

/**
 * Discards the local variables or upvalues created/captured in a loop scope.
 */
static void discardLocalsLoop(FalconCompiler *compiler) {
    FunctionCompiler *fCompiler = compiler->fCompiler;
    for (int i = fCompiler->localCount - 1;
         i >= 0 && fCompiler->locals[i].depth > fCompiler->loop->scopeDepth; i--) {
        if (fCompiler->locals[fCompiler->localCount - 1].isCaptured) {
            emitByte(compiler, OP_CLOSE_UPVALUE);
        } else {
            emitByte(compiler, OP_POP);
        }
    }
}

/**
 * Compiles a "break" control flow statement, breaking the current iteration of a loop and
 * discarding locals created.
 */
static void breakStatement(FalconCompiler *compiler) {
    FunctionCompiler *fCompiler = compiler->fCompiler;
    CHECK_LOOP_ERROR(fCompiler, FALCON_BREAK_LOOP_ERR); /* Checks if not inside a loop */
    consume(compiler, TK_SEMICOLON, FALCON_BREAK_STMT_ERR);
    discardLocalsLoop(compiler); /* Discards locals created in loop */

    /* Emits a temporary instruction to signal a "break" statement. It should become an "OP_JUMP"
     * instruction, once the size of the loop body is known */
    emitJump(compiler, OP_TEMP);
}

/**
 * Compiles a "next" control flow statement, advancing to the next iteration of a loop and
 * discarding locals created.
 */
static void nextStatement(FalconCompiler *compiler) {
    FunctionCompiler *fCompiler = compiler->fCompiler;
    CHECK_LOOP_ERROR(fCompiler, FALCON_NEXT_LOOP_ERR); /* Checks if not inside a loop */
    consume(compiler, TK_SEMICOLON, FALCON_NEXT_STMT_ERR);
    discardLocalsLoop(compiler); /* Discards locals created in loop */

    /* Jumps to the entry point of the current innermost loop */
    emitLoop(compiler, fCompiler->loop->entry);
}

#undef CHECK_LOOP_ERROR

/**
 * Compiles a "return" statement.
 */
static void returnStatement(FalconCompiler *compiler) {
    if (compiler->fCompiler->type == TYPE_SCRIPT) /* Checks if in top level code */
        compilerError(compiler, &compiler->parser->previous, FALCON_RETURN_TOP_LEVEL_ERR);

    if (match(compiler, TK_SEMICOLON)) {
        emitReturn(compiler);
    } else {
        expression(compiler);
        consume(compiler, TK_SEMICOLON, FALCON_RETURN_STMT_ERR);
        emitByte(compiler, OP_RETURN);
    }
}

/**
 * Compiles a statement.
 */
static void statement(FalconCompiler *compiler) {
    if (match(compiler, TK_IF)) {
        ifStatement(compiler);
    } else if (match(compiler, TK_SWITCH)) {
        switchStatement(compiler);
    } else if (match(compiler, TK_WHILE)) {
        whileStatement(compiler);
    } else if (match(compiler, TK_FOR)) {
        forStatement(compiler);
    } else if (match(compiler, TK_BREAK)) {
        breakStatement(compiler);
    } else if (match(compiler, TK_NEXT)) {
        nextStatement(compiler);
    } else if (match(compiler, TK_RETURN)) {
        returnStatement(compiler);
    } else if (match(compiler, TK_LEFT_BRACE)) {
        beginScope(compiler->fCompiler);
        block(compiler);
        endScope(compiler);
    } else {
        expressionStatement(compiler);
    }
}

/**
 * Synchronize error recovery (Panic Mode). Error recovery is used to minimize the number of
 * cascaded compile errors reported. While Panic Mode is active, the compiler will keep skipping
 * tokens until a synchronization point is reached - an expression end or a statement/declaration
 * begin.
 */
static void synchronize(FalconCompiler *compiler) {
    Parser *parser = compiler->parser;
    parser->panicMode = false;

    while (parser->current.type != TK_EOF) {
        if (parser->previous.type == TK_SEMICOLON) return; /* Sync point (expression end) */

        switch (parser->current.type) { /* Sync point (statement/declaration begin) */
            case TK_CLASS:
            case TK_FOR:
            case TK_FUNCTION:
            case TK_IF:
            case TK_NEXT:
            case TK_RETURN:
            case TK_SWITCH:
            case TK_VAR:
            case TK_WHILE:
                return;
            default:; /* Keep skipping tokens */
        }

        advance(compiler);
    }
}

/**
 * Compiles a declaration statement.
 */
static void declaration(FalconCompiler *compiler) {
    if (match(compiler, TK_VAR)) {
        varDeclaration(compiler);
        consume(compiler, TK_SEMICOLON, FALCON_VAR_DECL_ERR);
    } else if (match(compiler, TK_FUNCTION)) {
        funDeclaration(compiler);
    } else {
        statement(compiler);
    }

    if (compiler->parser->panicMode) synchronize(compiler);
}

/**
 * Compiles a given source code string. The parsing technique used is a Pratt parser, an improved
 * recursive descent parser that associates semantics with tokens instead of grammar rules.
 */
ObjFunction *falconCompile(FalconVM *vm, const char *source) {
    Parser parser;
    Scanner scanner;
    FalconCompiler programCompiler;
    FunctionCompiler funCompiler;

    /* Inits the parser, scanner, and compiler */
    initParser(&parser);
    falconInitScanner(source, &scanner);
    initCompiler(&programCompiler, vm, &parser, &scanner);
    initFunctionCompiler(&programCompiler, &funCompiler, NULL, TYPE_SCRIPT);

    advance(&programCompiler);                 /* Get the first token */
    while (!match(&programCompiler, TK_EOF)) { /* Main compiler loop */
        declaration(&programCompiler);         /* Falcon's grammar entry point */
    }

    ObjFunction *function = endFunctionCompiler(&programCompiler);
    return programCompiler.parser->hadError ? NULL : function;
}
