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

#ifdef FALCON_DEBUG_PRINT_CODE
#include <stdio.h>
#include "../lib/falcon_debug.h"
#endif

/* Compilation flags */
#define FALCON_ERROR_STATE      -1
#define FALCON_UNDEFINED_SCOPE  FALCON_ERROR_STATE
#define FALCON_UNRESOLVED_LOCAL FALCON_ERROR_STATE
#define FALCON_GLOBAL_SCOPE     0

/* Parser representation */
typedef struct {
    FalconToken current;  /* The last "lexed" token */
    FalconToken previous; /* The last consumed token */
    bool hadError;        /* Whether a syntax/compile error occurred or not */
    bool panicMode;       /* Whether the parser is in error recovery (Panic Mode) or not */
} FalconParser;

/* Precedence levels, from lowest to highest */
typedef enum {
    FALCON_PREC_NONE,
    FALCON_PREC_ASSIGN,  /* 1: "=", "-=", "+=", "*=", "/=", "%=" */
    FALCON_PREC_TERNARY, /* 2: "?:" */
    FALCON_PREC_OR,      /* 3: "or" */
    FALCON_PREC_AND,     /* 4: "and" */
    FALCON_PREC_EQUAL,   /* 5: "==", "!=" */
    FALCON_PREC_COMPARE, /* 6: "<", ">", "<=", ">=" */
    FALCON_PREC_TERM,    /* 7: "+", "-" */
    FALCON_PREC_FACTOR,  /* 8: "*", "/", "%" */
    FALCON_PREC_UNARY,   /* 9: "not", "-" */
    FALCON_PREC_POW,     /* 10: "^" */
    FALCON_PREC_POSTFIX  /* 11: ".", "()", "[]" */
} FalconPrecedence;

/* Program compiler representation */
typedef struct {
    FalconVM *vm;                      /* Falcon's virtual machine instance */
    FalconParser *parser;              /* Falcon's parser instance */
    FalconScanner *scanner;            /* Falcon's scanner instance */
    FalconFunctionCompiler *fCompiler; /* The compiler for the currently compiling function */
} FalconCompiler;

/* Function pointer to the parsing functions and a common interface to all parsing rules */
typedef void (*FalconParseFunction)(FalconCompiler *compiler, bool canAssign);
#define FALCON_PARSE_RULE(name) static void name(FalconCompiler *compiler, bool canAssign)

/* Parsing rules (prefix function, infix function, precedence level) */
typedef struct {
    FalconParseFunction prefix;
    FalconParseFunction infix;
    FalconPrecedence precedence;
} FalconParseRule;

/**
 * Initializes a given Parser instance as error-free and with no loops.
 */
static void initParser(FalconParser *parser) {
    parser->hadError = false;
    parser->panicMode = false;
}

/**
 * Presents a syntax/compiler error to the programmer.
 */
void compilerError(FalconCompiler *compiler, FalconToken *token, const char *message) {
    if (compiler->parser->panicMode) return; /* Checks and sets error recovery */
    compiler->parser->panicMode = true;
    FalconCompileTimeError(compiler->vm, compiler->scanner, token,
                           message); /* Presents the error */
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
        compiler->parser->current = FalconScanToken(compiler->scanner);
        if (compiler->parser->current.type != FALCON_TK_ERROR) break;
        compilerError(compiler, &compiler->parser->current, compiler->parser->current.start);
    }
}

/**
 * Reads the next token and validates that the token has an expected type. If not, reports an error.
 */
static void consume(FalconCompiler *compiler, FalconTokenType type, const char *message) {
    if (compiler->parser->current.type == type) {
        advance(compiler);
        return;
    }

    compilerError(compiler, &compiler->parser->current, message);
}

/**
 * Checks if the current token if of the given type.
 */
static bool check(FalconParser *parser, FalconTokenType type) {
    return parser->current.type == type;
}

/**
 * Checks if the current token is of the given type. If so, the token is consumed.
 */
static bool match(FalconCompiler *compiler, FalconTokenType type) {
    if (!check(compiler->parser, type)) return false;
    advance(compiler);
    return true;
}

/**
 * Returns the compiling function.
 */
static FalconObjFunction *currentFunction(FalconFunctionCompiler *fCompiler) {
    return fCompiler->function;
}

/**
 * Returns the compiling bytecode chunk.
 */
static FalconBytecodeChunk *currentBytecode(FalconFunctionCompiler *fCompiler) {
    return &currentFunction(fCompiler)->bytecodeChunk;
}

/**
 * Appends a single byte to the bytecode chunk.
 */
static void emitByte(FalconCompiler *compiler, uint8_t byte) {
    FalconWriteBytecode(compiler->vm, currentBytecode(compiler->fCompiler), byte,
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
    emitByte(compiler, FALCON_OP_LOOP);
    uint16_t offset = currentBytecode(compiler->fCompiler)->count - loopStart + 2;

    if (offset > UINT16_MAX) /* Loop is too long? */
        compilerError(compiler, &compiler->parser->previous, FALCON_LOOP_LIMIT_ERR);

    emitByte(compiler, (uint8_t) ((uint16_t) (offset >> 8u) & (uint16_t) 0xff));
    emitByte(compiler, (uint8_t) (offset & (uint16_t) 0xff));
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
    emitBytes(compiler, FALCON_OP_NULL, FALCON_OP_RETURN);
}

/**
 * Adds a constant to the bytecode chunk constants table.
 */
static uint8_t makeConstant(FalconCompiler *compiler, FalconValue value) {
    int constant = FalconAddConstant(compiler->vm, currentBytecode(compiler->fCompiler), value);
    if (constant > UINT8_MAX) {
        compilerError(compiler, &compiler->parser->previous, FALCON_CONST_LIMIT_ERR);
        return FALCON_ERROR_STATE;
    }

    return (uint8_t) constant;
}

/**
 * Inserts a constant in the constants table of the current bytecode chunk. Before inserting,
 * checks if the constant limit was exceeded.
 */
static void emitConstant(FalconCompiler *compiler, FalconValue value) {
    int constant = FalconAddConstant(compiler->vm, currentBytecode(compiler->fCompiler), value);
    if (constant > UINT16_MAX) {
        compilerError(compiler, &compiler->parser->previous, FALCON_CONST_LIMIT_ERR);
    } else {
        FalconWriteConstant(compiler->vm, currentBytecode(compiler->fCompiler), constant,
                            compiler->parser->previous.line);
    }
}

/**
 * Replaces the operand at the given location with the calculated jump offset. This function
 * should be called right before the emission of the next instruction that the jump should
 * land on.
 */
static void patchJump(FalconCompiler *compiler, int offset) {
    uint16_t jump =
        currentBytecode(compiler->fCompiler)->count - offset - 2; /* -2 to adjust by offset */

    if (jump > UINT16_MAX) /* Jump is too long? */
        compilerError(compiler, &compiler->parser->previous, FALCON_JUMP_LIMIT_ERR);

    currentBytecode(compiler->fCompiler)->code[offset] =
        (uint8_t)((uint16_t)(jump >> 8u) & (uint16_t) 0xff);
    currentBytecode(compiler->fCompiler)->code[offset + 1] = (uint8_t)(jump & (uint16_t) 0xff);
}

/**
 * Starts a new program compiler.
 */
static void initCompiler(FalconCompiler *compiler, FalconVM *vm, FalconParser *parser,
                         FalconScanner *scanner) {
    compiler->vm = vm;
    compiler->parser = parser;
    compiler->scanner = scanner;
}

/**
 * Starts a new function compilation process.
 */
static void initFunctionCompiler(FalconCompiler *compiler, FalconFunctionCompiler *fCompiler,
                                 FalconFunctionCompiler *enclosing, FalconFunctionType type) {
    fCompiler->enclosing = enclosing;
    fCompiler->function = NULL;
    fCompiler->loop = NULL;
    fCompiler->type = type;
    fCompiler->localCount = 0;
    fCompiler->scopeDepth = FALCON_GLOBAL_SCOPE;
    fCompiler->function = FalconNewFunction(compiler->vm);
    compiler->vm->compiler = compiler->fCompiler = fCompiler;

    FalconParser *parser = compiler->parser;
    if (type != FALCON_TYPE_SCRIPT)
        currentFunction(compiler->fCompiler)->name = FalconCopyString(
            compiler->vm, parser->previous.start, parser->previous.length); /* Sets function name */

    /* Set stack slot zero for the VM's internal use */
    FalconLocal *local = &compiler->fCompiler->locals[compiler->fCompiler->localCount++];
    local->depth = FALCON_GLOBAL_SCOPE;
    local->isCaptured = false;
    local->name.start = "";
    local->name.length = 0;
}

/**
 * Ends the function compilation process.
 */
static FalconObjFunction *endFunctionCompiler(FalconCompiler *compiler) {
    emitReturn(compiler);
    FalconObjFunction *function = currentFunction(compiler->fCompiler);

#ifdef FALCON_DEBUG_PRINT_CODE
    if (!compiler->parser->hadError) {
        FalconOpcodesHeader();
        FalconDisassembleBytecode(currentBytecode(compiler->fCompiler),
                                  function->name != NULL ? function->name->chars : FALCON_SCRIPT);
#ifdef FALCON_DEBUG_TRACE_EXECUTION
        printf("\n");
#endif
    }
#endif

    compiler->vm->compiler = compiler->fCompiler = compiler->fCompiler->enclosing;
    return function;
}

/**
 * Begins a new scope by incrementing the current scope depth.
 */
static void beginScope(FalconFunctionCompiler *fCompiler) { fCompiler->scopeDepth++; }

/**
 * Ends an existing scope by decrementing the current scope depth and popping all local variables
 * declared in the scope.
 */
static void endScope(FalconCompiler *compiler) {
    FalconFunctionCompiler *fCompiler = compiler->fCompiler;
    fCompiler->scopeDepth--;

    /* Closes locals and upvalues in the scope */
    while (fCompiler->localCount > 0 &&
           fCompiler->locals[fCompiler->localCount - 1].depth > fCompiler->scopeDepth) {
        if (fCompiler->locals[fCompiler->localCount - 1].isCaptured) {
            emitByte(compiler, FALCON_OP_CLOSE_UPVALUE);
        } else {
            emitByte(compiler, FALCON_OP_POP);
        }

        fCompiler->localCount--;
    }
}

/* Forward parser's declarations, since the grammar is recursive */
static void expression(FalconCompiler *compiler);
static void statement(FalconCompiler *compiler);
static void declaration(FalconCompiler *compiler);
static FalconParseRule *getParseRule(FalconTokenType type);
static void parsePrecedence(FalconCompiler *compiler, FalconPrecedence precedence);

/**
 * Checks if an identifier constant was already defined. If so, return its index. If not, set the
 * identifier in the global names table.
 */
static uint8_t identifierConstant(FalconCompiler *compiler, FalconToken *name) {
    return makeConstant(compiler,
                        FALCON_OBJ_VAL(FalconCopyString(compiler->vm, name->start, name->length)));
}

/**
 * Checks if two identifiers match.
 */
static bool identifiersEqual(FalconToken *a, FalconToken *b) {
    if (a->length != b->length) return false;
    return memcmp(a->start, b->start, (size_t) a->length) == 0;
}

/**
 * Resolves a local variable by looping through the list of locals that are currently in the scope.
 * If one has the same name as the identifier token, the variable is resolved.
 */
static int resolveLocal(FalconCompiler *compiler, FalconFunctionCompiler *fCompiler,
                        FalconToken *name) {
    for (int i = fCompiler->localCount - 1; i >= 0; i--) {
        FalconLocal *local = &fCompiler->locals[i];
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
static int addUpvalue(FalconCompiler *compiler, FalconFunctionCompiler *fCompiler, uint8_t index,
                      bool isLocal) {
    FalconObjFunction *function = currentFunction(fCompiler);
    int upvalueCount = function->upvalueCount;

    /* Checks if the upvalue is already in the upvalue list */
    for (int i = 0; i < upvalueCount; i++) {
        FalconUpvalue *upvalue = &fCompiler->upvalues[i];
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
static int resolveUpvalue(FalconCompiler *compiler, FalconFunctionCompiler *fCompiler,
                          FalconToken *name) {
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
static void addLocal(FalconCompiler *compiler, FalconToken name) {
    FalconFunctionCompiler *fCompiler = compiler->fCompiler;
    if (fCompiler->localCount == FALCON_MAX_BYTE) {
        compilerError(compiler, &compiler->parser->previous, FALCON_VAR_LIMIT_ERR);
        return;
    }

    FalconLocal *local = &fCompiler->locals[fCompiler->localCount++];
    local->name = name;
    local->depth = FALCON_UNDEFINED_SCOPE;
    local->isCaptured = false;
}

/**
 * Records the existence of a variable declaration.
 */
static void declareVariable(FalconCompiler *compiler) {
    FalconFunctionCompiler *fCompiler = compiler->fCompiler;
    if (fCompiler->scopeDepth == FALCON_GLOBAL_SCOPE) return; /* Globals are late bound, exit */
    FalconToken *name = &compiler->parser->previous;

    /* Verifies if local variable was previously declared */
    for (int i = fCompiler->localCount - 1; i >= 0; i--) {
        FalconLocal *local = &fCompiler->locals[i];
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
    consume(compiler, FALCON_TK_IDENTIFIER, errorMessage);
    declareVariable(compiler); /* Declares the variables */

    if (compiler->fCompiler->scopeDepth > FALCON_GLOBAL_SCOPE)
        return FALCON_ERROR_STATE; /* Locals are not looked up by name, exit */

    return identifierConstant(compiler, &compiler->parser->previous);
}

/**
 * Marks a local variable as initialized and available for use.
 */
static void markInitialized(FalconFunctionCompiler *fCompiler) {
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

    emitBytes(compiler, FALCON_OP_DEFINE_GLOBAL, global);
}

/**
 * Compiles the list of arguments in a function call.
 */
static uint8_t argumentList(FalconCompiler *compiler) {
    uint8_t argCount = 0;
    if (!check(compiler->parser, FALCON_TK_RIGHT_PAREN)) {
        do {
            expression(compiler);
            if (argCount == UINT8_MAX)
                compilerError(compiler, &compiler->parser->previous, FALCON_ARGS_LIMIT_ERR);
            argCount++;
        } while (match(compiler, FALCON_TK_COMMA));
    }

    consume(compiler, FALCON_TK_RIGHT_PAREN, FALCON_CALL_LIST_PAREN_ERR);
    return argCount;
}

/**
 * Emits the instructions for a given compound assignment, which provide a shorter syntax for
 * assigning the result of an arithmetic operator.
 */
static void compoundAssignment(FalconCompiler *compiler, uint8_t getOpcode, uint8_t setOpcode,
                               uint8_t arg, uint8_t opcode) {
    emitBytes(compiler, getOpcode, (uint8_t) arg);
    expression(compiler);
    emitByte(compiler, opcode);
    emitBytes(compiler, setOpcode, (uint8_t) arg);
}

/**
 * Gets the index of a variable in the constants table and emits the the bytecode to perform the
 * load of the global/local variable.
 */
static void namedVariable(FalconCompiler *compiler, FalconToken name, bool canAssign) {
    uint8_t getOpcode, setOpcode;
    int arg = resolveLocal(compiler, compiler->fCompiler, &name);

    /* Finds the current scope */
    if (arg != FALCON_UNRESOLVED_LOCAL) { /* Local variable? */
        getOpcode = FALCON_OP_GET_LOCAL;
        setOpcode = FALCON_OP_SET_LOCAL;
    } else if ((arg = resolveUpvalue(compiler, compiler->fCompiler, &name)) != -1) { /* Upvalue? */
        getOpcode = FALCON_OP_GET_UPVALUE;
        setOpcode = FALCON_OP_SET_UPVALUE;
    } else { /* Global variable */
        arg = identifierConstant(compiler, &name);
        getOpcode = FALCON_OP_GET_GLOBAL;
        setOpcode = FALCON_OP_SET_GLOBAL;
    }

    /* Compiles variable assignments or access */
    if (canAssign && match(compiler, FALCON_TK_EQUAL)) { /* a = ... */
        expression(compiler);
        emitBytes(compiler, setOpcode, (uint8_t) arg);
    } else if (canAssign && match(compiler, FALCON_TK_MINUS_EQUAL)) { /* a -= ... */
        compoundAssignment(compiler, getOpcode, setOpcode, arg, FALCON_OP_SUBTRACT);
    } else if (canAssign && match(compiler, FALCON_TK_PLUS_EQUAL)) { /* a += ... */
        compoundAssignment(compiler, getOpcode, setOpcode, arg, FALCON_OP_ADD);
    } else if (canAssign && match(compiler, FALCON_TK_DIV_EQUAL)) { /* a /= ... */
        compoundAssignment(compiler, getOpcode, setOpcode, arg, FALCON_OP_DIVIDE);
    } else if (canAssign && match(compiler, FALCON_TK_MOD_EQUAL)) { /* a %= ... */
        compoundAssignment(compiler, getOpcode, setOpcode, arg, FALCON_OP_MOD);
    } else if (canAssign && match(compiler, FALCON_TK_MULTIPLY_EQUAL)) { /* a *= ... */
        compoundAssignment(compiler, getOpcode, setOpcode, arg, FALCON_OP_MULTIPLY);
    } else if (canAssign && match(compiler, FALCON_TK_POW_EQUAL)) { /* a ^= ... */
        compoundAssignment(compiler, getOpcode, setOpcode, arg, FALCON_OP_POW);
    } else {  /* Access variable */
        emitBytes(compiler, getOpcode, (uint8_t) arg);
    }
}

/**
 * Handles the "and" logical operator with short-circuit.
 */
FALCON_PARSE_RULE(and_) {
    (void) canAssign; /* Unused */
    int jump = emitJump(compiler, FALCON_OP_AND);
    parsePrecedence(compiler, FALCON_PREC_AND);
    patchJump(compiler, jump);
}

/**
 * Handles the "or" logical operator with short-circuit.
 */
FALCON_PARSE_RULE(or_) {
    (void) canAssign; /* Unused */
    int jump = emitJump(compiler, FALCON_OP_OR);
    parsePrecedence(compiler, FALCON_PREC_OR);
    patchJump(compiler, jump);
}

/**
 * Handles a binary (infix) expression, by compiling the right operand of the expression (the left
 * one was already compiled). Then, emits the bytecode instruction that performs the binary
 * operation.
 */
FALCON_PARSE_RULE(binary) {
    (void) canAssign; /* Unused */
    FalconTokenType operatorType = compiler->parser->previous.type;
    FalconParseRule *rule = getParseRule(operatorType); /* Gets the current rule */
    parsePrecedence(compiler, rule->precedence + 1);    /* Compiles with the correct precedence */

    /* Emits the operator instruction */
    switch (operatorType) {
        case FALCON_TK_NOT_EQUAL:
            emitBytes(compiler, FALCON_OP_EQUAL, FALCON_OP_NOT);
            break;
        case FALCON_TK_EQUAL_EQUAL:
            emitByte(compiler, FALCON_OP_EQUAL);
            break;
        case FALCON_TK_GREATER:
            emitByte(compiler, FALCON_OP_GREATER);
            break;
        case FALCON_TK_GREATER_EQUAL:
            emitBytes(compiler, FALCON_OP_LESS, FALCON_OP_NOT);
            break;
        case FALCON_TK_LESS:
            emitByte(compiler, FALCON_OP_LESS);
            break;
        case FALCON_TK_LESS_EQUAL:
            emitBytes(compiler, FALCON_OP_GREATER, FALCON_OP_NOT);
            break;
        case FALCON_TK_PLUS:
            emitByte(compiler, FALCON_OP_ADD);
            break;
        case FALCON_TK_MINUS:
            emitByte(compiler, FALCON_OP_SUBTRACT);
            break;
        case FALCON_TK_DIV:
            emitByte(compiler, FALCON_OP_DIVIDE);
            break;
        case FALCON_TK_MOD:
            emitByte(compiler, FALCON_OP_MOD);
            break;
        case FALCON_TK_MULTIPLY:
            emitByte(compiler, FALCON_OP_MULTIPLY);
            break;
        default:
            return; /* Unreachable */
    }
}

/**
 * Handles a function call expression by parsing its arguments list and emitting the instruction to
 * proceed with the execution of the function.
 */
FALCON_PARSE_RULE(call) {
    (void) canAssign; /* Unused */
    uint8_t argCount = argumentList(compiler);
    emitBytes(compiler, FALCON_OP_CALL, argCount);
}

/**
 * Handles the opening parenthesis by compiling the expression between the parentheses, and then
 * parsing the closing parenthesis.
 */
FALCON_PARSE_RULE(grouping) {
    (void) canAssign; /* Unused */
    expression(compiler);
    consume(compiler, FALCON_TK_RIGHT_PAREN, FALCON_GRP_EXPR_ERR);
}

/**
 * Handles a literal (booleans or null) expression by outputting the proper instruction.
 */
FALCON_PARSE_RULE(literal) {
    (void) canAssign; /* Unused */
    switch (compiler->parser->previous.type) {
        case FALCON_TK_FALSE:
            emitByte(compiler, FALCON_OP_FALSE);
            break;
        case FALCON_TK_NULL:
            emitByte(compiler, FALCON_OP_NULL);
            break;
        case FALCON_TK_TRUE:
            emitByte(compiler, FALCON_OP_TRUE);
            break;
        default:
            return; /* Unreachable */
    }
}

/**
 * Handles a numeric expression by converting a string to a double number and then generates the
 * code to load that value by calling "emitConstant".
 */
FALCON_PARSE_RULE(number) {
    (void) canAssign; /* Unused */
    double value = strtod(compiler->parser->previous.start, NULL);
    emitConstant(compiler, FALCON_NUM_VAL(value));
}

/**
 * Handles a exponentiation expression.
 */
FALCON_PARSE_RULE(pow_) {
    (void) canAssign;                           /* Unused */
    parsePrecedence(compiler, FALCON_PREC_POW); /* Compiles the operand */
    emitByte(compiler, FALCON_OP_POW);
}

/**
 * Handles a string expression by creating a string object, wrapping it in a Value, and then
 * adding it to the constants table.
 */
FALCON_PARSE_RULE(string) {
    (void) canAssign; /* Unused */
    FalconParser *parser = compiler->parser;
    emitConstant(compiler, FALCON_OBJ_VAL(FalconCopyString(compiler->vm, parser->previous.start + 1,
                                                           parser->previous.length - 2)));
}

/**
 * Handles a variable access.
 */
FALCON_PARSE_RULE(variable) {
    namedVariable(compiler, compiler->parser->previous, canAssign);
}

/**
 * Handles the ternary "?:" conditional operator expression.
 */
FALCON_PARSE_RULE(ternary) {
    (void) canAssign;                                         /* Unused */
    int ifJump = emitJump(compiler, FALCON_OP_JUMP_IF_FALSE); /* Jumps if the condition is false */
    emitByte(compiler, FALCON_OP_POP);                        /* Pops the condition result */
    parsePrecedence(compiler, FALCON_PREC_TERNARY);           /* Compiles the first branch */
    consume(compiler, FALCON_TK_COLON, FALCON_TERNARY_EXPR_ERR);

    int elseJump =
        emitJump(compiler, FALCON_OP_JUMP);        /* Jumps the second branch if first was taken */
    patchJump(compiler, ifJump);                   /* Patches the jump over the first branch */
    emitByte(compiler, FALCON_OP_POP);             /* Pops the condition result */
    parsePrecedence(compiler, FALCON_PREC_ASSIGN); /* Compiles the second branch */
    patchJump(compiler, elseJump);                 /* Patches the jump over the second branch */
}

/**
 * Handles a unary expression by compiling the operand and then emitting the bytecode to perform
 * the unary operation itself.
 */
FALCON_PARSE_RULE(unary) {
    (void) canAssign; /* Unused */
    FalconTokenType operatorType = compiler->parser->previous.type;
    parsePrecedence(compiler, FALCON_PREC_UNARY); /* Compiles the operand */

    switch (operatorType) {
        case FALCON_TK_MINUS:
            emitByte(compiler, FALCON_OP_NEGATE);
            break;
        case FALCON_TK_NOT:
            emitByte(compiler, FALCON_OP_NOT);
            break;
        default:
            return;
    }
}

/* An array (table) of ParseRules that guides the Pratt parser. The left column represents the
 * prefix function parser; the second column represents the infix function parser; and the third
 * column represents the precedence level */
#define FALCON_EMPTY_RULE \
    { NULL, NULL, FALCON_PREC_NONE }
#define FALCON_PREFIX_RULE(prefix) \
    { prefix, NULL, FALCON_PREC_NONE }
#define FALCON_INFIX_RULE(infix, prec) \
    { NULL, infix, prec }
#define FALCON_RULE(prefix, infix, prec) \
    { prefix, infix, prec }

FalconParseRule rules[] = {
    FALCON_RULE(grouping, call, FALCON_PREC_POSTFIX), /* FALCON_TK_LEFT_PAREN */
    FALCON_EMPTY_RULE,                                /* FALCON_TK_RIGHT_PAREN */
    FALCON_EMPTY_RULE,                                /* FALCON_TK_LEFT_BRACE */
    FALCON_EMPTY_RULE,                                /* FALCON_TK_RIGHT_BRACE */
    FALCON_EMPTY_RULE,                                /* FALCON_TK_COMMA */
    FALCON_EMPTY_RULE,                                /* FALCON_TK_DOT */
    FALCON_EMPTY_RULE,                                /* FALCON_TK_COLON */
    FALCON_EMPTY_RULE,                                /* FALCON_TK_SEMICOLON */
    FALCON_EMPTY_RULE,                                /* FALCON_TK_ARROW */
    FALCON_RULE(unary, binary, FALCON_PREC_TERM),     /* FALCON_TK_MINUS */
    FALCON_EMPTY_RULE,                                /* FALCON_TK_MINUS_EQUAL */
    FALCON_INFIX_RULE(binary, FALCON_PREC_TERM),      /* FALCON_TK_PLUS */
    FALCON_EMPTY_RULE,                                /* FALCON_TK_PLUS_EQUAL */
    FALCON_INFIX_RULE(binary, FALCON_PREC_FACTOR),    /* FALCON_TK_DIV */
    FALCON_EMPTY_RULE,                                /* FALCON_TK_DIV_EQUAL */
    FALCON_INFIX_RULE(binary, FALCON_PREC_FACTOR),    /* FALCON_TK_MOD */
    FALCON_EMPTY_RULE,                                /* FALCON_TK_MOD_EQUAL */
    FALCON_INFIX_RULE(binary, FALCON_PREC_FACTOR),    /* FALCON_TK_MULTIPLY */
    FALCON_EMPTY_RULE,                                /* FALCON_TK_MULTIPLY_EQUAL */
    FALCON_INFIX_RULE(pow_, FALCON_PREC_POW),         /* FALCON_TK_POW */
    FALCON_EMPTY_RULE,                                /* FALCON_TK_POW_EQUAL */
    FALCON_PREFIX_RULE(unary),                        /* FALCON_TK_NOT */
    FALCON_INFIX_RULE(binary, FALCON_PREC_EQUAL),     /* FALCON_TK_NOT_EQUAL */
    FALCON_EMPTY_RULE,                                /* FALCON_TK_EQUAL */
    FALCON_INFIX_RULE(binary, FALCON_PREC_EQUAL),     /* FALCON_TK_EQUAL_EQUAL */
    FALCON_INFIX_RULE(binary, FALCON_PREC_COMPARE),   /* FALCON_TK_GREATER */
    FALCON_INFIX_RULE(binary, FALCON_PREC_COMPARE),   /* FALCON_TK_GREATER_EQUAL */
    FALCON_INFIX_RULE(binary, FALCON_PREC_COMPARE),   /* FALCON_TK_LESS */
    FALCON_INFIX_RULE(binary, FALCON_PREC_COMPARE),   /* FALCON_TK_LESS_EQUAL */
    FALCON_INFIX_RULE(and_, FALCON_PREC_AND),         /* FALCON_TK_AND */
    FALCON_INFIX_RULE(or_, FALCON_PREC_OR),           /* FALCON_TK_OR */
    FALCON_INFIX_RULE(ternary, FALCON_PREC_TERNARY),  /* FALCON_TK_TERNARY */
    FALCON_PREFIX_RULE(variable),                     /* FALCON_TK_IDENTIFIER */
    FALCON_PREFIX_RULE(string),                       /* FALCON_TK_STRING */
    FALCON_PREFIX_RULE(number),                       /* FALCON_TK_NUMBER */
    FALCON_EMPTY_RULE,                                /* FALCON_TK_BREAK */
    FALCON_EMPTY_RULE,                                /* FALCON_TK_CLASS */
    FALCON_EMPTY_RULE,                                /* FALCON_TK_ELSE */
    FALCON_PREFIX_RULE(literal),                      /* FALCON_TK_FALSE */
    FALCON_EMPTY_RULE,                                /* FALCON_TK_FOR */
    FALCON_EMPTY_RULE,                                /* FALCON_TK_FUNCTION */
    FALCON_EMPTY_RULE,                                /* FALCON_TK_IF */
    FALCON_EMPTY_RULE,                                /* FALCON_TK_NEXT */
    FALCON_PREFIX_RULE(literal),                      /* FALCON_TK_NULL */
    FALCON_EMPTY_RULE,                                /* FALCON_TK_RETURN */
    FALCON_EMPTY_RULE,                                /* FALCON_TK_SUPER */
    FALCON_EMPTY_RULE,                                /* FALCON_TK_SWITCH */
    FALCON_EMPTY_RULE,                                /* FALCON_TK_THIS */
    FALCON_PREFIX_RULE(literal),                      /* FALCON_TK_TRUE */
    FALCON_EMPTY_RULE,                                /* FALCON_TK_VAR */
    FALCON_EMPTY_RULE,                                /* FALCON_TK_WHEN */
    FALCON_EMPTY_RULE,                                /* FALCON_TK_WHILE */
    FALCON_EMPTY_RULE,                                /* FALCON_TK_ERROR */
    FALCON_EMPTY_RULE                                 /* FALCON_TK_EOF */
};

#undef FALCON_EMPTY_RULE
#undef FALCON_PREFIX_RULE
#undef FALCON_INFIX_RULE
#undef FALCON_RULE

/**
 * Parses any expression of a given precedence level or higher. Reads the next token and looks up
 * the corresponding ParseRule. If there is no prefix parser, then the token must be a syntax error.
 */
static void parsePrecedence(FalconCompiler *compiler, FalconPrecedence precedence) {
    advance(compiler);
    FalconParser *parser = compiler->parser;
    FalconParseFunction prefixRule = getParseRule(parser->previous.type)->prefix;

    if (prefixRule == NULL) { /* Checks if is a parsing error */
        compilerError(compiler, &parser->previous, FALCON_EXPR_ERR);
        return;
    }

    /* Checks if the left side is assignable */
    bool canAssign = precedence <= FALCON_PREC_TERNARY;
    prefixRule(compiler, canAssign);

    /* Looks for an infix parser for the next token */
    while (precedence <= getParseRule(parser->current.type)->precedence) {
        advance(compiler);
        FalconParseFunction infixRule = getParseRule(parser->previous.type)->infix;
        infixRule(compiler, canAssign);
    }

    if (canAssign && match(compiler, FALCON_TK_EQUAL)) { /* Checks if is an invalid assignment */
        compilerError(compiler, &parser->previous, FALCON_INV_ASSG_ERR);
    }
}

/**
 * Returns the rule at a given index.
 */
static FalconParseRule *getParseRule(FalconTokenType type) { return &rules[type]; }

/**
 * Compiles an expression by parsing the lowest precedence level, which subsumes all of the higher
 * precedence expressions too.
 */
static void expression(FalconCompiler *compiler) { parsePrecedence(compiler, FALCON_PREC_ASSIGN); }

/**
 * Compiles a block of code by parsing declarations and statements until a closing brace (end of
 * block) is found.
 */
static void block(FalconCompiler *compiler) {
    while (!check(compiler->parser, FALCON_TK_RIGHT_BRACE) && !check(compiler->parser, FALCON_TK_EOF)) {
        declaration(compiler);
    }

    consume(compiler, FALCON_TK_RIGHT_BRACE, FALCON_BLOCK_BRACE_ERR);
}

/**
 * Compiles the declaration of a single variable.
 */
static void singleVarDeclaration(FalconCompiler *compiler) {
    uint8_t global = parseVariable(compiler, FALCON_VAR_NAME_ERR); /* Parses a variable name */

    if (match(compiler, FALCON_TK_EQUAL)) {
        expression(compiler); /* Compiles the variable initializer */
    } else {
        emitByte(compiler, FALCON_OP_NULL); /* Default variable value is "null" */
    }

    defineVariable(compiler, global); /* Emits the declaration bytecode */
}

/**
 * Compiles a variable declaration list.
 */
static void varDeclaration(FalconCompiler *compiler) {
    FalconParser *parser = compiler->parser;
    if (!check(parser, FALCON_TK_SEMICOLON)) {
        do {
            singleVarDeclaration(compiler); /* Compiles the declaration */
        } while (match(compiler, FALCON_TK_COMMA));
    }
}

/**
 * Compiles a function body and its parameters.
 */
static void function(FalconCompiler *compiler, FalconFunctionType type) {
    FalconParser *parser = compiler->parser;
    FalconFunctionCompiler fCompiler;
    initFunctionCompiler(compiler, &fCompiler, compiler->fCompiler, type);
    beginScope(compiler->fCompiler);

    /* Compiles the parameter list */
    consume(compiler, FALCON_TK_LEFT_PAREN, FALCON_FUNC_NAME_PAREN_ERR);
    if (!check(parser, FALCON_TK_RIGHT_PAREN)) {
        do {
            FalconObjFunction *current = currentFunction(compiler->fCompiler);
            current->arity++;

            if (current->arity > UINT8_MAX) /* Exceeds the limit os parameters? */
                compilerError(compiler, &parser->current, FALCON_PARAMS_LIMIT_ERR);

            uint8_t paramConstant = parseVariable(compiler, FALCON_PARAM_NAME_ERR);
            defineVariable(compiler, paramConstant);
        } while (match(compiler, FALCON_TK_COMMA));
    }
    consume(compiler, FALCON_TK_RIGHT_PAREN, FALCON_FUNC_LIST_PAREN_ERR);

    /* Compiles the function body */
    consume(compiler, FALCON_TK_LEFT_BRACE, FALCON_FUNC_BODY_BRACE_ERR);
    block(compiler);

    /* Create the function object */
    FalconObjFunction *function = endFunctionCompiler(compiler);
    emitBytes(compiler, FALCON_OP_CLOSURE, makeConstant(compiler, FALCON_OBJ_VAL(function)));

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
    function(compiler, FALCON_TYPE_FUNCTION);
    defineVariable(compiler, func);
}

/**
 * Compiles an expression statement.
 */
static void expressionStatement(FalconCompiler *compiler) {
    expression(compiler);
    consume(compiler, FALCON_TK_SEMICOLON, FALCON_EXPR_STMT_ERR);
//    emitByte(compiler->parser, (compiler->vm->isREPL) ? FALCON_OP_POP_EXPR : FALCON_OP_POP);
    emitByte(compiler, FALCON_OP_POP);
}

/**
 * Compiles an "if" conditional statement.
 */
static void ifStatement(FalconCompiler *compiler) {
    expression(compiler); /* Compiles condition */
    consume(compiler, FALCON_TK_LEFT_BRACE, FALCON_IF_STMT_ERR);

    int thenJump = emitJump(compiler, FALCON_OP_JUMP_IF_FALSE);
    emitByte(compiler, FALCON_OP_POP);

    /* Compiles the "if" block */
    beginScope(compiler->fCompiler);
    block(compiler);
    endScope(compiler);

    int elseJump = emitJump(compiler, FALCON_OP_JUMP);
    patchJump(compiler, thenJump);
    emitByte(compiler, FALCON_OP_POP);

    /* Compiles the "else" block */
    if (match(compiler, FALCON_TK_ELSE)) {
        if (match(compiler, FALCON_TK_IF)) {
            ifStatement(compiler); /* "else if ..." form */
        } else if (match(compiler, FALCON_TK_LEFT_BRACE)) {
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
    FalconParser *parser = compiler->parser;

    /* Possible switch states */
    typedef enum {
        FALCON_BEF_CASES, /* Before all cases */
        FALCON_BEF_ELSE,  /* Before else case */
        FALCON_AFT_ELSE   /* After else case */
    } SwitchState;

    SwitchState switchState = FALCON_BEF_CASES;
    int caseEnds[FALCON_MAX_BYTE];
    int caseCount = 0;
    int previousCaseSkip = -1;

    expression(compiler); /* Compiles expression to switch on */
    consume(compiler, FALCON_TK_LEFT_BRACE, FALCON_SWITCH_STMT_ERR);

    while (!match(compiler, FALCON_TK_RIGHT_BRACE) && !check(parser, FALCON_TK_EOF)) {
        if (match(compiler, FALCON_TK_WHEN) || match(compiler, FALCON_TK_ELSE)) {
            FalconTokenType caseType = parser->previous.type;

            if (switchState == FALCON_AFT_ELSE) { /* Already compiled the else case? */
                compilerError(compiler, &parser->previous, FALCON_ELSE_END_ERR);
            } else if (switchState == FALCON_BEF_ELSE) { /* Else case not compiled yet? */
                caseEnds[caseCount++] =
                    emitJump(compiler, FALCON_OP_JUMP); /* Jumps the other cases */
                patchJump(compiler, previousCaseSkip);  /* Patches the jump */
                emitByte(compiler, FALCON_OP_POP);
            }

            if (caseType == FALCON_TK_WHEN) {
                switchState = FALCON_BEF_ELSE;

                /* Checks if the case is equal to the switch value */
                emitByte(compiler, FALCON_OP_DUP); /* "==" pops its operand, so duplicate before */
                expression(compiler);
                consume(compiler, FALCON_TK_ARROW, FALCON_ARR_CASE_ERR);
                emitByte(compiler, FALCON_OP_EQUAL);
                previousCaseSkip = emitJump(compiler, FALCON_OP_JUMP_IF_FALSE);

                emitByte(compiler, FALCON_OP_POP); /* Pops the comparison result */
            } else {
                switchState = FALCON_AFT_ELSE;
                consume(compiler, FALCON_TK_ARROW, FALCON_ARR_ELSE_ERR);
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
        emitByte(compiler, FALCON_OP_POP);
    }

    /* Patch all the case jumps to the end */
    for (int i = 0; i < caseCount; i++) {
        patchJump(compiler, caseEnds[i]);
    }

    emitByte(compiler, FALCON_OP_POP); /* Pops the switch value */
}

/* Starts the compilation of a new loop by setting the entry point to the current bytecode chunk
 * instruction */
#define FALCON_START_LOOP(fCompiler)                \
    FalconLoop loop;                                \
    loop.enclosing = fCompiler->loop;               \
    loop.entry = currentBytecode(fCompiler)->count; \
    loop.scopeDepth = fCompiler->scopeDepth;        \
    fCompiler->loop = &loop

/* Compiles the body of a loop and sets its index */
#define FALCON_LOOP_BODY(compiler)                                                        \
    compiler->fCompiler->loop->body = compiler->fCompiler->function->bytecodeChunk.count; \
    block(compiler)

/**
 * Gets the number of arguments for a instruction at the given program counter.
 */
int instructionArgs(const FalconBytecodeChunk *bytecodeChunk, int pc) {
    switch (bytecodeChunk->code[pc]) {
        case FALCON_OP_FALSE:
        case FALCON_OP_TRUE:
        case FALCON_OP_NULL:
        case FALCON_OP_NOT:
        case FALCON_OP_EQUAL:
        case FALCON_OP_GREATER:
        case FALCON_OP_LESS:
        case FALCON_OP_ADD:
        case FALCON_OP_SUBTRACT:
        case FALCON_OP_NEGATE:
        case FALCON_OP_DIVIDE:
        case FALCON_OP_MOD:
        case FALCON_OP_MULTIPLY:
        case FALCON_OP_POW:
        case FALCON_OP_CLOSE_UPVALUE:
        case FALCON_OP_RETURN:
        case FALCON_OP_DUP:
        case FALCON_OP_POP:
        case FALCON_OP_POP_EXPR:
        case FALCON_OP_TEMP:
            return 0; /* Instructions with no arguments */

        case FALCON_OP_DEFINE_GLOBAL:
        case FALCON_OP_GET_GLOBAL:
        case FALCON_OP_SET_GLOBAL:
        case FALCON_OP_GET_UPVALUE:
        case FALCON_OP_SET_UPVALUE:
        case FALCON_OP_GET_LOCAL:
        case FALCON_OP_SET_LOCAL:
        case FALCON_OP_CALL:
            return 1; /* Instructions with single byte as argument */

        case FALCON_OP_CONSTANT:
        case FALCON_OP_AND:
        case FALCON_OP_OR:
        case FALCON_OP_JUMP:
        case FALCON_OP_JUMP_IF_FALSE:
        case FALCON_OP_LOOP:
            return 2; /* Instructions with 2 bytes as arguments */

        case FALCON_OP_CLOSURE: {
            int index = bytecodeChunk->code[pc + 1];
            FalconObjFunction *function =
                FALCON_AS_FUNCTION(bytecodeChunk->constants.values[index]);
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
    FalconFunctionCompiler *fCompiler = compiler->fCompiler;
    FalconBytecodeChunk *bytecodeChunk = &fCompiler->function->bytecodeChunk;
    int index = fCompiler->loop->body;

    while (index < bytecodeChunk->count) {
        if (bytecodeChunk->code[index] == FALCON_OP_TEMP) { /* Is a temporary for a "break"? */
            bytecodeChunk->code[index] = FALCON_OP_JUMP; /* Set the correct "OP_JUMP" instruction */
            patchJump(compiler, index + 1);              /* Patch the jump to the end of the loop */
            index += 3;
        } else { /* Jumps the instruction and its arguments */
            index += 1 + instructionArgs(bytecodeChunk, index); /* +1 byte - instruction */
        }
    }

    fCompiler->loop = fCompiler->loop->enclosing;
}

/**
 * Compiles a "while" loop statement.
 */
static void whileStatement(FalconCompiler *compiler) {
    FalconFunctionCompiler *fCompiler = compiler->fCompiler;

    FALCON_START_LOOP(fCompiler); /* Starts a bew loop */
    expression(compiler);         /* Compiles the loop condition */
    consume(compiler, FALCON_TK_LEFT_BRACE, FALCON_WHILE_STMT_ERR);
    int exitJump = emitJump(compiler, FALCON_OP_JUMP_IF_FALSE);
    emitByte(compiler, FALCON_OP_POP);

    /* Compiles the "while" block */
    beginScope(fCompiler);
    FALCON_LOOP_BODY(compiler);
    endScope(compiler);

    /* Emits the loop and patches the next jump */
    emitLoop(compiler, fCompiler->loop->entry);
    patchJump(compiler, exitJump);
    emitByte(compiler, FALCON_OP_POP);
    endLoop(compiler); /* Ends the loop */
}

/**
 * Compiles a "for" loop statement.
 */
static void forStatement(FalconCompiler *compiler) {
    FalconFunctionCompiler *fCompiler = compiler->fCompiler;
    beginScope(compiler->fCompiler); /* Begins the loop scope */

    /* Compiles the initializer clause */
    if (match(compiler, FALCON_TK_COMMA)) { /* Empty initializer? */
        compilerError(compiler, &compiler->parser->previous, FALCON_FOR_STMT_INIT_ERR);
    } else {
        singleVarDeclaration(compiler); /* Variable declaration initializer */
        consume(compiler, FALCON_TK_COMMA, FALCON_FOR_STMT_CM1_ERR);
    }

    FALCON_START_LOOP(fCompiler); /* Starts a bew loop */

    /* Compiles the conditional clause */
    expression(compiler);
    consume(compiler, FALCON_TK_COMMA, FALCON_FOR_STMT_CM2_ERR);
    int exitJump = emitJump(compiler, FALCON_OP_JUMP_IF_FALSE);
    emitByte(compiler, FALCON_OP_POP); /* Pops condition */

    /* Compiles the increment clause */
    int bodyJump = emitJump(compiler, FALCON_OP_JUMP);
    int incrementStart = currentBytecode(fCompiler)->count;
    expression(compiler);
    emitByte(compiler, FALCON_OP_POP); /* Pops increment */
    consume(compiler, FALCON_TK_LEFT_BRACE, FALCON_FOR_STMT_BRC_ERR);
    emitLoop(compiler, fCompiler->loop->entry);
    fCompiler->loop->entry = incrementStart;
    patchJump(compiler, bodyJump);

    FALCON_LOOP_BODY(compiler); /* Compiles the "for" block */
    emitLoop(compiler, fCompiler->loop->entry);
    if (exitJump != -1) {
        patchJump(compiler, exitJump);
        emitByte(compiler, FALCON_OP_POP); /* Pops condition */
    }

    endScope(compiler); /* Ends the loop scope */
    endLoop(compiler);  /* Ends the loop */
}

#undef FALCON_START_LOOP
#undef FALCON_LOOP_BODY

/* Checks if the current scope is outside of a loop body */
#define FALCON_CHECK_LOOP_ERROR(fCompiler, error)                        \
    do {                                                                 \
        if (fCompiler->loop == NULL) /* Is outside of a loop body? */    \
            compilerError(compiler, &compiler->parser->previous, error); \
    } while (false)

/**
 * Discards the local variables or upvalues created/captured in a loop scope.
 */
static void discardLocalsLoop(FalconCompiler *compiler) {
    FalconFunctionCompiler *fCompiler = compiler->fCompiler;
    for (int i = fCompiler->localCount - 1;
         i >= 0 && fCompiler->locals[i].depth > fCompiler->loop->scopeDepth; i--) {
        if (fCompiler->locals[fCompiler->localCount - 1].isCaptured) {
            emitByte(compiler, FALCON_OP_CLOSE_UPVALUE);
        } else {
            emitByte(compiler, FALCON_OP_POP);
        }
    }
}

/**
 * Compiles a "break" control flow statement, breaking the current iteration of a loop and
 * discarding locals created.
 */
static void breakStatement(FalconCompiler *compiler) {
    FalconFunctionCompiler *fCompiler = compiler->fCompiler;
    FALCON_CHECK_LOOP_ERROR(fCompiler, FALCON_BREAK_LOOP_ERR); /* Checks if not inside a loop */
    consume(compiler, FALCON_TK_SEMICOLON, FALCON_BREAK_STMT_ERR);
    discardLocalsLoop(compiler); /* Discards locals created in loop */

    /* Emits a temporary instruction to signal a "break" statement. It should become an "OP_JUMP"
     * instruction, once the size of the loop body is known */
    emitJump(compiler, FALCON_OP_TEMP);
}

/**
 * Compiles a "next" control flow statement, advancing to the next iteration of a loop and
 * discarding locals created.
 */
static void nextStatement(FalconCompiler *compiler) {
    FalconFunctionCompiler *fCompiler = compiler->fCompiler;
    FALCON_CHECK_LOOP_ERROR(fCompiler, FALCON_NEXT_LOOP_ERR); /* Checks if not inside a loop */
    consume(compiler, FALCON_TK_SEMICOLON, FALCON_NEXT_STMT_ERR);
    discardLocalsLoop(compiler); /* Discards locals created in loop */

    /* Jumps to the entry point of the current innermost loop */
    emitLoop(compiler, fCompiler->loop->entry);
}

#undef CHECK_LOOP_ERROR

/**
 * Compiles a "return" statement.
 */
static void returnStatement(FalconCompiler *compiler) {
    if (compiler->fCompiler->type == FALCON_TYPE_SCRIPT) /* Checks if in top level code */
        compilerError(compiler, &compiler->parser->previous, FALCON_RETURN_TOP_LEVEL_ERR);

    if (match(compiler, FALCON_TK_SEMICOLON)) {
        emitReturn(compiler);
    } else {
        expression(compiler);
        consume(compiler, FALCON_TK_SEMICOLON, FALCON_RETURN_STMT_ERR);
        emitByte(compiler, FALCON_OP_RETURN);
    }
}

/**
 * Compiles a statement.
 */
static void statement(FalconCompiler *compiler) {
    if (match(compiler, FALCON_TK_IF)) {
        ifStatement(compiler);
    } else if(match(compiler, FALCON_TK_SWITCH)) {
        switchStatement(compiler);
    } else if (match(compiler, FALCON_TK_WHILE)) {
        whileStatement(compiler);
    } else if (match(compiler, FALCON_TK_FOR)) {
        forStatement(compiler);
    } else if (match(compiler, FALCON_TK_BREAK)) {
        breakStatement(compiler);
    } else if (match(compiler, FALCON_TK_NEXT)) {
        nextStatement(compiler);
    } else if (match(compiler, FALCON_TK_RETURN)) {
        returnStatement(compiler);
    } else if (match(compiler, FALCON_TK_LEFT_BRACE)) {
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
    FalconParser *parser = compiler->parser;
    parser->panicMode = false;

    while (parser->current.type != FALCON_TK_EOF) {
        if (parser->previous.type == FALCON_TK_SEMICOLON)
            return; /* Sync point (expression end) */

        switch (parser->current.type) { /* Sync point (statement/declaration begin) */
            case FALCON_TK_CLASS:
            case FALCON_TK_FOR:
            case FALCON_TK_FUNCTION:
            case FALCON_TK_IF:
            case FALCON_TK_NEXT:
            case FALCON_TK_RETURN:
            case FALCON_TK_SWITCH:
            case FALCON_TK_VAR:
            case FALCON_TK_WHILE:
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
    if (match(compiler, FALCON_TK_VAR)) {
        varDeclaration(compiler);
        consume(compiler, FALCON_TK_SEMICOLON, FALCON_VAR_DECL_ERR);
    } else if (match(compiler, FALCON_TK_FUNCTION)) {
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
FalconObjFunction *FalconCompile(FalconVM *vm, const char *source) {
    FalconParser parser;
    FalconScanner scanner;
    FalconCompiler programCompiler;
    FalconFunctionCompiler funCompiler;

    /* Inits the parser, scanner, and compiler */
    initParser(&parser);
    FalconInitScanner(source, &scanner);
    initCompiler(&programCompiler, vm, &parser, &scanner);
    initFunctionCompiler(&programCompiler, &funCompiler, NULL, FALCON_TYPE_SCRIPT);

    advance(&programCompiler);                        /* Get the first token */
    while (!match(&programCompiler, FALCON_TK_EOF)) { /* Main compiler loop */
        declaration(&programCompiler);                /* Falcon's grammar entry point */
    }

    FalconObjFunction *function = endFunctionCompiler(&programCompiler);
    return programCompiler.parser->hadError ? NULL : function;
}
