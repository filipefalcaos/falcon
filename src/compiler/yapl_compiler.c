/*
 * YAPL version 0.0.1 (master, Oct 18 2019)
 * yapl_compiler.c: YAPL's handwritten parser/compiler based on Pratt Parsing
 * See YAPL's license in the LICENSE file
 */

#include "yapl_compiler.h"
#include "../lib/string/yapl_string.h"
#include "../lib/yapl_error.h"
#include <stdlib.h>
#include <string.h>

#ifdef YAPL_DEBUG_PRINT_CODE
#include "../lib/yapl_debug.h"
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
    PREC_ASSIGN,  /* '=' */
    PREC_OR,      /* 'or' */
    PREC_AND,     /* 'and' */
    PREC_EQUAL,   /* '==', '!=' */
    PREC_COMPARE, /* '<', '>', '<=', '>=' */
    PREC_TERM,    /* '+', '-' */
    PREC_FACTOR,  /* '*', '/', '%' */
    PREC_UNARY,   /* '!', '-', '++', '--' */
    PREC_POSTFIX  /* '.', '()', '[]' */
} Precedence;

/* Function pointer to parse functions */
typedef void (*ParseFunction)(Parser *parser, Scanner *scanner, bool canAssign);

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
    bool isCaptured;
} Local;

/* Upvalue representation */
typedef struct {
    uint8_t index;
    bool isLocal;
} Upvalue;

/* YAPL's function types */
typedef enum { TYPE_FUNCTION, TYPE_SCRIPT } FunctionType;

/* YAPL's compiler representation */
typedef struct {
    Parser *parser;
    Scanner *scanner;
} ProgramCompiler;

/* YAPL's compiler representation */
typedef struct FunctionCompiler {
    struct FunctionCompiler *enclosing;
    ObjFunction *function;
    FunctionType type;
    Local locals[MAX_SINGLE_BYTE]; // TODO: make it possible to have more than 256
    Upvalue upvalues[MAX_SINGLE_BYTE];
    int localCount;
    int scopeDepth;
} FunctionCompiler;

/* Parser instance */
FunctionCompiler *functionCompiler = NULL;

/**
 * Returns the compiling bytecode chunk.
 */
static BytecodeChunk *currentBytecodeChunk() { return &functionCompiler->function->bytecodeChunk; }

/**
 * Presents a syntax/compiler error to the programmer.
 */
void compilerError(Parser *parser, Scanner *scanner, Token *token, const char *message) {
    if (parser->panicMode) return; /* Checks and sets error recovery */
    parser->panicMode = true;
    compileTimeError(scanner, token, message); /* Presents the error */
    parser->hadError = true;
}

/**
 * Takes the old current token and then loops through the token stream and to get the next token.
 * The loop keeps going reading tokens and reporting the errors, until it hits a non-error one or
 * reach EOF.
 */
static void advance(Parser *parser, Scanner *scanner) {
    parser->previous = parser->current;

    while (true) {
        parser->current = scanToken(scanner);
        if (parser->current.type != TK_ERROR) break;
        compilerError(parser, scanner, &parser->current, parser->current.start);
    }
}

/**
 * Reads the next token and validates that the token has an expected type. If not, reports an error.
 */
static void consume(Parser *parser, Scanner *scanner, TokenType type, const char *message) {
    if (parser->current.type == type) {
        advance(parser, scanner);
        return;
    }

    compilerError(parser, scanner, &parser->current, message);
}

/**
 * Checks if the current token if of the given type.
 */
static bool check(Parser *parser, TokenType type) { return parser->current.type == type; }

/**
 * Checks if the current token is of the given type. If so, the token is consumed.
 */
static bool match(Parser *parser, Scanner *scanner, TokenType type) {
    if (!check(parser, type)) return false;
    advance(parser, scanner);
    return true;
}

/**
 * Appends a single byte to the bytecode chunk.
 */
static void emitByte(Parser *parser, uint8_t byte) {
    writeToBytecodeChunk(currentBytecodeChunk(), byte, parser->previous.line);
}

/**
 * Appends two bytes (operation code and operand) to the bytecode chunk.
 */
static void emitBytes(Parser *parser, uint8_t byte_1, uint8_t byte_2) {
    emitByte(parser, byte_1);
    emitByte(parser, byte_2);
}

/**
 * Emits a new 'loop back' instruction which jumps backwards by a given offset.
 */
static void emitLoop(Parser *parser, Scanner *scanner, int loopStart) {
    emitByte(parser, OP_LOOP);
    int offset = currentBytecodeChunk()->count - loopStart + 2;
    if (offset > UINT16_MAX) compilerError(parser, scanner, &parser->previous, LOOP_LIMIT_ERR);
    emitByte(parser, (uint8_t) ((offset >> 8) & 0xff));
    emitByte(parser, (uint8_t) (offset & 0xff));
}

/**
 * Emits the bytecode of a given instruction and reserve two bytes for a jump offset.
 */
static int emitJump(Parser *parser, uint8_t instruction) {
    emitByte(parser, instruction);
    emitByte(parser, 0xff);
    emitByte(parser, 0xff);
    return currentBytecodeChunk()->count - 2;
}

/**
 * Emits the OP_RETURN bytecode instruction.
 */
static void emitReturn(Parser *parser) { emitBytes(parser, OP_NULL, OP_RETURN); }

/**
 * Adds a constant to the bytecode chunk constants table.
 */
static uint8_t makeConstant(Parser *parser, Scanner *scanner, Value value) {
    int constant = addConstant(currentBytecodeChunk(), value);
    if (constant > UINT8_MAX) {
        compilerError(parser, scanner, &parser->previous, CONST_LIMIT_ERR);
        return 0;
    }

    return (uint8_t) constant;
}

/**
 * Inserts a constant in the constants table of the current bytecode chunk. Before inserting,
 * checks if the constant limit was exceeded.
 */
static void emitConstant(Parser *parser, Scanner *scanner, Value value) {
    emitBytes(parser, OP_CONSTANT, makeConstant(parser, scanner, value));
}

/**
 * Replaces the operand at the given location with the calculated jump offset. This function
 * should be called right before the emission of the next instruction that the jump should
 * land on.
 */
static void patchJump(Parser *parser, Scanner *scanner, int offset) {
    int jump = currentBytecodeChunk()->count - offset - 2; /* -2 to adjust by offset */
    if (jump > UINT16_MAX) compilerError(parser, scanner, &parser->previous, JUMP_LIMIT_ERR);
    currentBytecodeChunk()->code[offset] = (uint8_t) ((jump >> 8) & 0xff);
    currentBytecodeChunk()->code[offset + 1] = (uint8_t) (jump & 0xff);
}

/**
 * Starts a new program compiler.
 */
static void initProgramCompiler(ProgramCompiler *compiler, Scanner *scanner, Parser *parser) {
    compiler->scanner = scanner;
    compiler->parser = parser;
}

/**
 * Starts a new compilation process.
 */
static void initCompiler(Parser *parser, FunctionCompiler *compiler, FunctionType type) {
    compiler->enclosing = functionCompiler;
    compiler->function = NULL;
    compiler->type = type;
    compiler->localCount = 0;
    compiler->scopeDepth = 0;
    compiler->function = newFunction();
    functionCompiler = compiler;

    if (type != TYPE_SCRIPT)
        functionCompiler->function->name =
            copyString(parser->previous.start, parser->previous.length); /* Sets function name */

    /* Set stack slot zero for the VM's internal use */
    Local *local = &functionCompiler->locals[functionCompiler->localCount++];
    local->depth = 0;
    local->isCaptured = false;
    local->name.start = "";
    local->name.length = 0;
}

/**
 * Ends the compilation process.
 */
static ObjFunction *endCompiler(Parser *parser) {
    emitReturn(parser);
    ObjFunction *function = functionCompiler->function;

#ifdef YAPL_DEBUG_PRINT_CODE
    if (!parser.hadError) {
        printOpcodesHeader();
        disassembleBytecodeChunk(currentBytecodeChunk(),
                                 function->name != NULL ? function->name->chars : SCRIPT_TAG);
#ifdef YAPL_DEBUG_TRACE_EXECUTION
        printf("\n");
#endif
    }
#endif

    functionCompiler = functionCompiler->enclosing;
    return function;
}

/**
 * Begins a new scope by incrementing the current scope depth.
 */
static void beginScope() { functionCompiler->scopeDepth++; }

/**
 * Ends an existing scope by decrementing the current scope depth and popping all local variables
 * declared in the scope.
 */
static void endScope(Parser *parser) {
    functionCompiler->scopeDepth--;

    /* Closes locals and upvalues in the scope */
    while (functionCompiler->localCount > 0 &&
           functionCompiler->locals[functionCompiler->localCount - 1].depth >
               functionCompiler->scopeDepth) {
        if (functionCompiler->locals[functionCompiler->localCount - 1].isCaptured) {
            emitByte(parser, OP_CLOSE_UPVALUE);
        } else {
            emitByte(parser, OP_POP);
        }

        functionCompiler->localCount--;
    }
}

/* Forward parser's declarations, since the grammar is recursive */
static void expression(Parser *parser, Scanner *scanner);
static void statement(Parser *parser, Scanner *scanner);
static void declaration(Parser *parser, Scanner *scanner);
static ParseRule *getRule(TokenType type);
static void parsePrecedence(Parser *parser, Scanner* scanner, Precedence precedence);

/**
 * Checks if an identifier constant was already defined. If so, return its index. If not, set the
 * identifier in the global names table.
 */
static uint8_t identifierConstant(Parser *parser, Scanner *scanner, Token *name) {
    return makeConstant(parser, scanner, OBJ_VAL(copyString(name->start, name->length)));
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
static int resolveLocal(Parser *parser, Scanner *scanner, FunctionCompiler *compiler, Token *name) {
    for (int i = compiler->localCount - 1; i >= 0; i--) {
        Local *local = &compiler->locals[i];
        if (identifiersEqual(name, &local->name)) { /* Checks if identifier matches */
            if (local->depth == -1)
                compilerError(parser, scanner, &parser->previous, RED_INIT_ERR);
            return i;
        }
    }

    return -1;
}

/**
 * Adds a new upvalue to the upvalue list and returns the index of the created upvalue.
 */
static int addUpvalue(Parser *parser, Scanner *scanner, FunctionCompiler *compiler, uint8_t index,
                      bool isLocal) {
    int upvalueCount = compiler->function->upvalueCount;

    /* Checks if the upvalue is already in the upvalue list */
    for (int i = 0; i < upvalueCount; i++) {
        Upvalue *upvalue = &compiler->upvalues[i];
        if (upvalue->index == index && upvalue->isLocal == isLocal) return i;
    }

    if (upvalueCount == MAX_SINGLE_BYTE) {
        compilerError(parser, scanner, &parser->previous, CLOSURE_LIMIT_ERR);
        return 0;
    }

    /* Adds the new upvalue */
    compiler->upvalues[upvalueCount].isLocal = isLocal;
    compiler->upvalues[upvalueCount].index = index;
    return compiler->function->upvalueCount++;
}

/**
 * Resolves an upvalue by looking for a local variable declared in any of the surrounding scopes. If
 * found, an upvalue index for that variable is returned.
 */
static int resolveUpvalue(Parser *parser, Scanner *scanner, FunctionCompiler *compiler,
                          Token *name) {
    if (compiler->enclosing == NULL) return -1; /* Global variable */

    /* Looks for a local variable in the enclosing scope */
    int local = resolveLocal(parser, scanner, compiler->enclosing, name);
    if (local != -1) {
        compiler->enclosing->locals[local].isCaptured = true;
        return addUpvalue(parser, scanner, compiler, (uint8_t) local, true);
    }

    /* Looks for an upvalue in the enclosing scope */
    int upvalue = resolveUpvalue(parser, scanner, compiler->enclosing, name);
    if (upvalue != -1) return addUpvalue(parser, scanner, compiler, (uint8_t)upvalue, false);

    return -1;
}

/**
 * Adds a local variable to the list of variables in a scope depth.
 */
static void addLocal(Parser *parser, Scanner *scanner, Token name) {
    if (functionCompiler->localCount == MAX_SINGLE_BYTE) {
        compilerError(parser, scanner, &parser->previous, VAR_LIMIT_ERR);
        return;
    }

    Local *local = &functionCompiler->locals[functionCompiler->localCount++];
    local->name = name;
    local->depth = -1;
    local->isCaptured = false;
}

/**
 * Records the existence of a variable declaration.
 */
static void declareVariable(Parser *parser, Scanner *scanner) {
    if (functionCompiler->scopeDepth == 0) return; /* Globals are late bound, exit */
    Token *name = &parser->previous;

    /* Verifies if variable was previously declared */
    for (int i = functionCompiler->localCount - 1; i >= 0; i--) {
        Local *local = &functionCompiler->locals[i];
        if (local->depth != -1 && local->depth < functionCompiler->scopeDepth) break;
        if (identifiersEqual(name, &local->name)) /* Checks if already declared */
            compilerError(parser, scanner, &parser->previous, VAR_REDECL_ERR);
    }

    addLocal(parser, scanner, *name);
}

/**
 * Parses a variable by consuming an identifier token and then adds the the token lexeme to the
 * constants table.
 */
static uint8_t parseVariable(Parser *parser, Scanner *scanner, const char *errorMessage) {
    consume(parser, scanner, TK_IDENTIFIER, errorMessage);
    declareVariable(parser, scanner);               /* Declares the variables */
    if (functionCompiler->scopeDepth > 0) return 0; /* Locals are not looked up by name, exit */
    return identifierConstant(parser, scanner, &parser->previous);
}

/**
 * Marks a local variable as initialized and available for use.
 */
static void markInitialized() {
    if (functionCompiler->scopeDepth == 0) return;
    functionCompiler->locals[functionCompiler->localCount - 1].depth = functionCompiler->scopeDepth;
}

/**
 * Handles a variable declaration by emitting the bytecode to perform a global variable declaration.
 */
static void defineVariable(Parser *parser, uint8_t global) {
    if (functionCompiler->scopeDepth > 0) {
        markInitialized(); /* Mark as initialized */
        return;            /* Only globals are defined at runtime */
    }

    emitBytes(parser, OP_DEFINE_GLOBAL, global);
}

/**
 * Compiles the list of arguments in a function call.
 */
static uint8_t argumentList(Parser *parser, Scanner *scanner) {
    uint8_t argCount = 0;

    if (!check(parser, TK_RIGHT_PAREN)) {
        do {
            expression(parser, scanner);
            if (argCount == 255) compilerError(parser, scanner, &parser->previous, ARGS_LIMIT_ERR);
            argCount++;
        } while (match(parser, scanner, TK_COMMA));
    }

    consume(parser, scanner, TK_RIGHT_PAREN, CALL_LIST_PAREN_ERR);
    return argCount;
}

/**
 * Handles the "and" logical operator with short-circuit.
 */
static void and_(Parser *parser, Scanner *scanner, bool canAssign) {
    int jump = emitJump(parser, OP_AND);
    parsePrecedence(parser, scanner, PREC_AND);
    patchJump(parser, scanner, jump);
}

/**
 * Handles the "or" logical operator with short-circuit.
 */
static void or_(Parser *parser, Scanner *scanner, bool canAssign) {
    int jump = emitJump(parser, OP_OR);
    parsePrecedence(parser, scanner, PREC_OR);
    patchJump(parser, scanner, jump);
}

/**
 * Handles a binary (infix) expression, by compiling the right operand of the expression (the left
 * one was already compiled). Then, emits the bytecode instruction that performs the binary
 * operation.
 */
static void binary(Parser *parser, Scanner *scanner, bool canAssign) {
    TokenType operatorType = parser->previous.type;
    ParseRule *rule = getRule(operatorType); /* Gets the current rule */
    parsePrecedence(parser, scanner,
                    rule->precedence + 1); /* Compiles with the correct precedence */

    /* Emits the operator instruction */
    switch (operatorType) {
        case TK_NOT_EQUAL:
            emitBytes(parser, OP_EQUAL, OP_NOT);
            break;
        case TK_EQUAL_EQUAL:
            emitByte(parser, OP_EQUAL);
            break;
        case TK_GREATER:
            emitByte(parser, OP_GREATER);
            break;
        case TK_GREATER_EQUAL:
            emitBytes(parser, OP_LESS, OP_NOT);
            break;
        case TK_LESS:
            emitByte(parser, OP_LESS);
            break;
        case TK_LESS_EQUAL:
            emitBytes(parser, OP_GREATER, OP_NOT);
            break;
        case TK_PLUS:
            emitByte(parser, OP_ADD);
            break;
        case TK_MINUS:
            emitByte(parser, OP_SUBTRACT);
            break;
        case TK_DIV:
            emitByte(parser, OP_DIVIDE);
            break;
        case TK_MOD:
            emitByte(parser, OP_MOD);
            break;
        case TK_MULTIPLY:
            emitByte(parser, OP_MULTIPLY);
            break;
        default:
            return; /* Unreachable */
    }
}

/**
 * Handles a function call expression by parsing its arguments list and emitting the instruction to
 * proceed with the execution of the function.
 */
static void call(Parser *parser, Scanner *scanner, bool canAssign) {
    uint8_t argCount = argumentList(parser, scanner);
    emitBytes(parser, OP_CALL, argCount);
}

/**
 * Handles a literal (booleans or null) expression by outputting the proper instruction.
 */
static void literal(Parser *parser, Scanner *scanner, bool canAssign) {
    switch (parser->previous.type) {
        case TK_FALSE:
            emitByte(parser, OP_FALSE);
            break;
        case TK_NULL:
            emitByte(parser, OP_NULL);
            break;
        case TK_TRUE:
            emitByte(parser, OP_TRUE);
            break;
        default:
            return; /* Unreachable */
    }
}

/**
 * Handles the opening parenthesis by compiling the expression between the parentheses, and then
 * parsing the closing parenthesis.
 */
static void grouping(Parser *parser, Scanner *scanner, bool canAssign) {
    expression(parser, scanner);
    consume(parser, scanner, TK_RIGHT_PAREN, GRP_EXPR_ERR);
}

/**
 * Handles a numeric expression by converting a string to a double number and then generates the
 * code to load that value by calling "emitConstant".
 */
static void number(Parser *parser, Scanner *scanner, bool canAssign) {
    double value = strtod(parser->previous.start, NULL);
    emitConstant(parser, scanner, NUM_VAL(value));
}

/**
 * Handles a string expression by creating a string object, wrapping it in a Value, and then
 * adding it to the constants table.
 */
static void string(Parser *parser, Scanner *scanner, bool canAssign) {
    emitConstant(parser, scanner,
                 OBJ_VAL(copyString(parser->previous.start + 1, parser->previous.length - 2)));
}

/**
 * Gets the index of a variable in the constants table and emits the the bytecode to perform the
 * load of the global/local variable.
 */
static void namedVariable(Parser *parser, Scanner *scanner, Token name, bool canAssign) {
    uint8_t getOpcode, setOpcode;
    int arg = resolveLocal(parser, scanner, functionCompiler, &name);

    /* Finds the current scope */
    if (arg != -1) { /* Local variable? */
        getOpcode = OP_GET_LOCAL;
        setOpcode = OP_SET_LOCAL;
    } else if ((arg = resolveUpvalue(parser, scanner, functionCompiler, &name)) !=
               -1) { /* Upvalue? */
        getOpcode = OP_GET_UPVALUE;
        setOpcode = OP_SET_UPVALUE;
    } else { /* Global variable */
        arg = identifierConstant(parser, scanner, &name);
        getOpcode = OP_GET_GLOBAL;
        setOpcode = OP_SET_GLOBAL;
    }

    if (canAssign && match(parser, scanner, TK_EQUAL)) {
        expression(parser, scanner);
        emitBytes(parser, setOpcode, (uint8_t) arg);
    } else {
        emitBytes(parser, getOpcode, (uint8_t) arg);
    }
}

/**
 * Handles a variable access.
 */
static void variable(Parser *parser, Scanner *scanner, bool canAssign) {
    namedVariable(parser, scanner, parser->previous, canAssign);
}

/**
 * Handles a prefix '++' or '--' operator.
 */
static void prefix(Parser *parser, Scanner *scanner, bool canAssign) {
    TokenType operatorType = parser->previous.type;
    Token name = parser->current;

    consume(parser, scanner, TK_IDENTIFIER, VAR_NAME_ERR);  /* Variable name expected */
    namedVariable(parser, scanner, parser->previous, true); /* Loads variable */

    switch (operatorType) { /* Gets prefix operation */
        case TK_INCREMENT:
            emitByte(parser, OP_INCREMENT);
            break;
        case TK_DECREMENT:
            emitByte(parser, OP_DECREMENT);
            break;
        default:
            return;
    }

    int index = resolveLocal(parser, scanner, functionCompiler, &name);
    uint8_t setOp;

    if (index != -1) { /* Local variable */
        setOp = OP_SET_LOCAL;
    } else { /* Global variable */
        index = identifierConstant(parser, scanner, &name);
        setOp = OP_SET_GLOBAL;
    }

    emitBytes(parser, setOp, index);
}

/**
 * Handles a unary expression by compiling the operand and then emitting the bytecode to perform
 * the unary operation itself.
 */
static void unary(Parser *parser, Scanner *scanner, bool canAssign) {
    TokenType operatorType = parser->previous.type;
    parsePrecedence(parser, scanner, PREC_UNARY); /* Compiles the operand */

    switch (operatorType) {
        case TK_MINUS:
            emitByte(parser, OP_NEGATE);
            break;
        case TK_NOT:
            emitByte(parser, OP_NOT);
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
    PREFIX_RULE(prefix),                /* TK_DECREMENT */
    PREFIX_RULE(prefix),                /* TK_INCREMENT */
    EMPTY_RULE,                         /* TK_PLUS_EQUAL */
    EMPTY_RULE,                         /* TK_MINUS_EQUAL */
    EMPTY_RULE,                         /* TK_MULTIPLY_EQUAL */
    EMPTY_RULE,                         /* TK_DIV_EQUAL */
    EMPTY_RULE,                         /* TK_MOD_EQUAL */
    INFIX_RULE(and_, PREC_AND),         /* TK_AND */
    INFIX_RULE(or_, PREC_OR),           /* TK_OR */
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
static void parsePrecedence(Parser *parser, Scanner *scanner, Precedence precedence) {
    advance(parser, scanner);
    ParseFunction prefixRule = getRule(parser->previous.type)->prefix;

    if (prefixRule == NULL) { /* Checks if is a parsing error */
        compilerError(parser, scanner, &parser->previous, EXPR_ERR);
        return;
    }

    /* Checks if the left side is assignable */
    bool canAssign = precedence <= PREC_ASSIGN;
    prefixRule(parser, scanner, canAssign);

    /* Looks for an infix parser for the next token */
    while (precedence <= getRule(parser->current.type)->precedence) {
        advance(parser, scanner);
        ParseFunction infixRule = getRule(parser->previous.type)->infix;
        infixRule(parser, scanner, canAssign);
    }

    if (canAssign && match(parser, scanner, TK_EQUAL)) { /* Checks if is an invalid assignment */
        compilerError(parser, scanner, &parser->previous, INV_ASSG_ERR);
        expression(parser, scanner);
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
void expression(Parser *parser, Scanner *scanner) { parsePrecedence(parser, scanner, PREC_ASSIGN); }

/**
 * Compiles a block of code by parsing declarations and statements until a closing brace (end of
 * block) is found.
 */
static void block(Parser *parser, Scanner *scanner) {
    while (!check(parser, TK_RIGHT_BRACE) && !check(parser, TK_EOF)) {
        declaration(parser, scanner);
    }

    consume(parser, scanner, TK_RIGHT_BRACE, BLOCK_BRACE_ERR);
}

/**
 * Compiles a variable declaration list.
 */
static void varDeclaration(Parser *parser, Scanner *scanner) {
    if (!check(parser, TK_SEMICOLON)) {
        do {
            uint8_t global =
                parseVariable(parser, scanner, VAR_NAME_ERR); /* Parses a variable name */
            if (match(parser, scanner, TK_EQUAL)) {
                expression(parser, scanner); /* Compiles the variable initializer */
            } else {
                emitByte(parser, OP_NULL);
            }
            defineVariable(parser, global); /* Emits the declaration bytecode */
        } while (match(parser, scanner, TK_COMMA));
    }

    consume(parser, scanner, TK_SEMICOLON, VAR_DECL_ERR);
}

/**
 * Compiles a function body and its parameters.
 */
static void function(Parser *parser, Scanner *scanner, FunctionType type) {
    FunctionCompiler compiler;
    initCompiler(parser, &compiler, type);
    beginScope();

    /* Compiles the parameter list */
    consume(parser, scanner, TK_LEFT_PAREN, FUNC_NAME_PAREN_ERR);
    if (!check(parser, TK_RIGHT_PAREN)) {
        do {
            functionCompiler->function->arity++;
            if (functionCompiler->function->arity > 255)
                compilerError(parser, scanner, &parser->current, PARAMS_LIMIT_ERR);
            uint8_t paramConstant = parseVariable(parser, scanner, PARAM_NAME_ERR);
            defineVariable(parser, paramConstant);
        } while (match(parser, scanner, TK_COMMA));
    }
    consume(parser, scanner, TK_RIGHT_PAREN, FUNC_LIST_PAREN_ERR);

    /* Compiles the function body */
    consume(parser, scanner, TK_LEFT_BRACE, FUNC_BODY_BRACE_ERR);
    block(parser, scanner);

    /* Create the function object */
    ObjFunction *function = endCompiler(parser);
    emitBytes(parser, OP_CLOSURE, makeConstant(parser, scanner, OBJ_VAL(function)));

    /* Emits the captured upvalues */
    for (int i = 0; i < function->upvalueCount; i++) {
        emitByte(parser, compiler.upvalues[i].isLocal ? 1 : 0);
        emitByte(parser, compiler.upvalues[i].index);
    }
}

/**
 * Compiles a function declaration.
 */
static void funDeclaration(Parser *parser, Scanner *scanner) {
    uint8_t global = parseVariable(parser, scanner, FUNC_NAME_ERR);
    markInitialized();
    function(parser, scanner, TYPE_FUNCTION);
    defineVariable(parser, global);
}

/**
 * Compiles an expression statement.
 */
static void expressionStatement(Parser *parser, Scanner *scanner) {
    expression(parser, scanner);
    consume(parser, scanner, TK_SEMICOLON, EXPR_STMT_ERR);
    emitByte(parser, OP_POP);
}

/**
 * Compiles an "if" conditional statement.
 */
static void ifStatement(Parser *parser, Scanner *scanner) {
    expression(parser, scanner); /* Compiles condition */
    consume(parser, scanner, TK_LEFT_BRACE, IF_STMT_ERR);
    int thenJump = emitJump(parser, OP_JUMP_IF_FALSE);
    emitByte(parser, OP_POP);

    /* Compiles the "if" block */
    beginScope();
    block(parser, scanner);
    endScope(parser);

    int elseJump = emitJump(parser, OP_JUMP);
    patchJump(parser, scanner, thenJump);
    emitByte(parser, OP_POP);

    /* Compiles the "else" block */
    if (match(parser, scanner, TK_ELSE)) {
        if (match(parser, scanner, TK_IF)) {
            ifStatement(parser, scanner); /* "else if ..." form */
        } else if (match(parser, scanner, TK_LEFT_BRACE)) {
            beginScope();
            block(parser, scanner); /* Compiles the "else" branch */
            endScope(parser);
        }
    }

    patchJump(parser, scanner, elseJump);
}

/**
 * Compiles a "while" loop statement.
 */
static void whileStatement(Parser *parser, Scanner *scanner) {
    int loopStart = currentBytecodeChunk()->count; /* Loop entry point */
    expression(parser, scanner);                   /* Compiles condition */
    consume(parser, scanner, TK_LEFT_BRACE, WHILE_STMT_ERR);
    int exitJump = emitJump(parser, OP_JUMP_IF_FALSE);
    emitByte(parser, OP_POP);

    /* Compiles the "while" block */
    beginScope();
    block(parser, scanner);
    endScope(parser);

    emitLoop(parser, scanner, loopStart);
    patchJump(parser, scanner, exitJump);
    emitByte(parser, OP_POP);
}

/**
 * Compiles a "for" loop statement.
 */
static void forStatement(Parser *parser, Scanner *scanner) {
    beginScope(); /* Begins the loop scope */

    /* Compiles the initializer clause */
    if (match(parser, scanner, TK_SEMICOLON)) {
        /* Empty initializer */
    } else if (match(parser, scanner, TK_VAR)) {
        varDeclaration(parser, scanner); /* Variable declaration initializer */
    } else {
        expressionStatement(parser, scanner); /* Expression initializer */
    }

    int loopStart = currentBytecodeChunk()->count; /* Loop entry point */
    int exitJump = -1;

    /* Compiles the conditional clause */
    if (!match(parser, scanner, TK_SEMICOLON)) {
        expression(parser, scanner);
        consume(parser, scanner, TK_SEMICOLON, FOR_STMT_COND_ERR);
        exitJump = emitJump(parser, OP_JUMP_IF_FALSE);
        emitByte(parser, OP_POP); /* Pops condition */
    }

    /* Compiles the increment clause */
    if (!match(parser, scanner, TK_LEFT_BRACE)) {
        int bodyJump = emitJump(parser, OP_JUMP);
        int incrementStart = currentBytecodeChunk()->count;
        expression(parser, scanner);
        emitByte(parser, OP_POP); /* Pops increment */
        consume(parser, scanner, TK_LEFT_BRACE, FOR_STMT_INC_ERR);
        emitLoop(parser, scanner, loopStart);
        loopStart = incrementStart;
        patchJump(parser, scanner, bodyJump);
    }

    block(parser, scanner); /* Compiles the "for" block */

    emitLoop(parser, scanner, loopStart);
    if (exitJump != -1) {
        patchJump(parser, scanner, exitJump);
        emitByte(parser, OP_POP); /* Pops condition */
    }

    endScope(parser); /* Ends the loop scope */
}


/**
 * Compiles a "return" statement.
 */
static void returnStatement(Parser *parser, Scanner *scanner) {
    if (functionCompiler->type == TYPE_SCRIPT) /* Checks if in top level code */
        compilerError(parser, scanner, &parser->previous, RETURN_TOP_LEVEL_ERR);

    if (match(parser, scanner, TK_SEMICOLON)) {
        emitReturn(parser);
    } else {
        expression(parser, scanner);
        consume(parser, scanner, TK_SEMICOLON, RETURN_STMT_ERR);
        emitByte(parser, OP_RETURN);
    }
}

/**
 * Compiles a statement.
 */
static void statement(Parser *parser, Scanner *scanner) {
    if (match(parser, scanner, TK_IF)) {
        ifStatement(parser, scanner);
    } else if (match(parser, scanner, TK_WHILE)) {
        whileStatement(parser, scanner);
    } else if (match(parser, scanner, TK_FOR)) {
        forStatement(parser, scanner);
    } else if (match(parser, scanner, TK_RETURN)) {
        returnStatement(parser, scanner);
    } else if (match(parser, scanner, TK_LEFT_BRACE)) {
        beginScope();
        block(parser, scanner);
        endScope(parser);
    } else {
        expressionStatement(parser, scanner);
    }
}

/**
 * Synchronize error recovery.
 */
static void synchronize(Parser *parser, Scanner *scanner) {
    parser->panicMode = false;

    while (parser->current.type != TK_EOF) {
        if (parser->previous.type == TK_SEMICOLON)
            return; /* Sync point (expression end) */

        switch (parser->current.type) { /* Sync point (expression begin) */
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

        advance(parser, scanner);
    }
}

/**
 * Compiles a declaration statement.
 */
static void declaration(Parser *parser, Scanner *scanner) {
    if (match(parser, scanner, TK_VAR)) {
        varDeclaration(parser, scanner);
    } else if (match(parser, scanner, TK_FUNCTION)) {
        funDeclaration(parser, scanner);
    } else {
        statement(parser, scanner);
    }

    if (parser->panicMode) synchronize(parser, scanner);
}

/**
 * Compiles a given source code string. The parsing technique used is a Pratt parser, an improved
 * recursive descent parser that associates semantics with tokens instead of grammar rules.
 */
ObjFunction *compile(const char *source) {
    Scanner scanner;
    Parser parser;
    ProgramCompiler proCompiler;
    FunctionCompiler funCompiler;

    /* Inits the compiler */
    initScanner(source, &scanner);
    initProgramCompiler(&proCompiler, &scanner, &parser);
    initCompiler(proCompiler.parser, &funCompiler, TYPE_SCRIPT);

    /* No panic mode yet */
    proCompiler.parser->hadError = false;
    proCompiler.parser->panicMode = false;

    advance(proCompiler.parser, proCompiler.scanner);                 /* Get the first token */
    while (!match(proCompiler.parser, proCompiler.scanner, TK_EOF)) { /* Main compiler loop */
        declaration(proCompiler.parser, proCompiler.scanner); /* YAPL's grammar entry point */
    }

    ObjFunction *function = endCompiler(proCompiler.parser);
    return proCompiler.parser->hadError ? NULL : function;
}
