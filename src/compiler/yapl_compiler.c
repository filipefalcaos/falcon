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

/* Compilation flags */
#define ERROR_STATE      -1
#define NO_LOOP          ERROR_STATE
#define UNDEFINED_SCOPE  ERROR_STATE
#define UNRESOLVED_LOCAL ERROR_STATE
#define GLOBAL_SCOPE     0

/* YAPL's parser representation */
typedef struct {
    Token current;
    Token previous;
    int innermostLoopStart;
    int innermostLoopScopeDepth;
    bool hadError;
    bool panicMode;
} Parser;

/* YAPL's precedence levels, from lowest to highest */
typedef enum {
    PREC_NONE,
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
    PREC_POSTFIX  /* 11: ".", "()", "[]" */
} Precedence;

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
    VM *vm;
    Parser *parser;
    Scanner *scanner;
} ProgramCompiler;

/* Function pointer to parse functions */
typedef void (*ParseFunction)(ProgramCompiler *compiler, bool canAssign);

/* YAPL's parsing rules (prefix function, infix function, precedence function) */
typedef struct {
    ParseFunction prefix;
    ParseFunction infix;
    Precedence precedence;
} ParseRule;

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

/* Compiler instance */
FunctionCompiler *functionCompiler = NULL;

/**
 * Returns the compiling bytecode chunk.
 */
static BytecodeChunk *currentBytecodeChunk() { return &functionCompiler->function->bytecodeChunk; }

/**
 * Initializes a given Parser instance as error-free and with no loops.
 */
static void initParser(Parser *parser) {
    parser->innermostLoopStart = NO_LOOP;
    parser->innermostLoopScopeDepth = GLOBAL_SCOPE;
    parser->hadError = false;
    parser->panicMode = false;
}

/**
 * Presents a syntax/compiler error to the programmer.
 */
void compilerError(ProgramCompiler *compiler, Token *token, const char *message) {
    if (compiler->parser->panicMode) return; /* Checks and sets error recovery */
    compiler->parser->panicMode = true;
    compileTimeError(compiler->vm, compiler->scanner, token, message); /* Presents the error */
    compiler->parser->hadError = true;
}

/**
 * Takes the old current token and then loops through the token stream and to get the next token.
 * The loop keeps going reading tokens and reporting the errors, until it hits a non-error one or
 * reach EOF.
 */
static void advance(ProgramCompiler *compiler) {
    compiler->parser->previous = compiler->parser->current;

    while (true) {
        compiler->parser->current = scanToken(compiler->scanner);
        if (compiler->parser->current.type != TK_ERROR) break;
        compilerError(compiler, &compiler->parser->current, compiler->parser->current.start);
    }
}

/**
 * Reads the next token and validates that the token has an expected type. If not, reports an error.
 */
static void consume(ProgramCompiler *compiler, TokenType type, const char *message) {
    if (compiler->parser->current.type == type) {
        advance(compiler);
        return;
    }

    compilerError(compiler, &compiler->parser->current, message);
}

/**
 * Checks if the current token if of the given type.
 */
static bool check(Parser *parser, TokenType type) { return parser->current.type == type; }

/**
 * Checks if the current token is of the given type. If so, the token is consumed.
 */
static bool match(ProgramCompiler *compiler, TokenType type) {
    if (!check(compiler->parser, type)) return false;
    advance(compiler);
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
static void emitLoop(ProgramCompiler *compiler, int loopStart) {
    emitByte(compiler->parser, OP_LOOP);
    int offset = currentBytecodeChunk()->count - loopStart + 2;
    if (offset > UINT16_MAX) compilerError(compiler, &compiler->parser->previous, LOOP_LIMIT_ERR);
    emitByte(compiler->parser, (uint8_t) ((offset >> 8) & 0xff));
    emitByte(compiler->parser, (uint8_t) (offset & 0xff));
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
static uint8_t makeConstant(ProgramCompiler *compiler, Value value) {
    int constant = addConstant(currentBytecodeChunk(), value);
    if (constant > UINT8_MAX) {
        compilerError(compiler, &compiler->parser->previous, CONST_LIMIT_ERR);
        return ERROR_STATE;
    }

    return (uint8_t) constant;
}

/**
 * Inserts a constant in the constants table of the current bytecode chunk. Before inserting,
 * checks if the constant limit was exceeded.
 */
static void emitConstant(ProgramCompiler *compiler, Value value) {
    int constant = addConstant(currentBytecodeChunk(), value);
    if (constant > UINT16_MAX) {
        compilerError(compiler, &compiler->parser->previous, CONST_LIMIT_ERR);
    } else {
        writeConstant(currentBytecodeChunk(), constant, compiler->parser->previous.line);
    }
}

/**
 * Replaces the operand at the given location with the calculated jump offset. This function
 * should be called right before the emission of the next instruction that the jump should
 * land on.
 */
static void patchJump(ProgramCompiler *compiler, int offset) {
    int jump = currentBytecodeChunk()->count - offset - 2; /* -2 to adjust by offset */
    if (jump > UINT16_MAX) compilerError(compiler, &compiler->parser->previous, JUMP_LIMIT_ERR);
    currentBytecodeChunk()->code[offset] = (uint8_t) ((jump >> 8) & 0xff);
    currentBytecodeChunk()->code[offset + 1] = (uint8_t) (jump & 0xff);
}

/**
 * Starts a new program compiler.
 */
static void initProgramCompiler(ProgramCompiler *compiler, VM *vm, Parser *parser,
                                Scanner *scanner) {
    compiler->vm = vm;
    compiler->parser = parser;
    compiler->scanner = scanner;
}

/**
 * Starts a new compilation process.
 */
static void initCompiler(ProgramCompiler *programCompiler, FunctionCompiler *compiler,
                         FunctionType type) {
    compiler->enclosing = functionCompiler;
    compiler->function = NULL;
    compiler->type = type;
    compiler->localCount = 0;
    compiler->scopeDepth = GLOBAL_SCOPE;
    compiler->function = newFunction(programCompiler->vm);
    functionCompiler = compiler;

    Parser *parser = programCompiler->parser;
    if (type != TYPE_SCRIPT)
        functionCompiler->function->name =
            copyString(programCompiler->vm, parser->previous.start,
                       parser->previous.length); /* Sets function name */

    /* Set stack slot zero for the VM's internal use */
    Local *local = &functionCompiler->locals[functionCompiler->localCount++];
    local->depth = GLOBAL_SCOPE;
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
    if (!parser->hadError) {
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
static void expression(ProgramCompiler *compiler);
static void statement(ProgramCompiler *compiler);
static void declaration(ProgramCompiler *compiler);
static ParseRule *getRule(TokenType type);
static void parsePrecedence(ProgramCompiler *compiler, Precedence precedence);

/**
 * Checks if an identifier constant was already defined. If so, return its index. If not, set the
 * identifier in the global names table.
 */
static uint8_t identifierConstant(ProgramCompiler *compiler, Token *name) {
    return makeConstant(compiler, OBJ_VAL(copyString(compiler->vm, name->start, name->length)));
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
static int resolveLocal(ProgramCompiler *programCompiler, FunctionCompiler *compiler, Token *name) {
    for (int i = compiler->localCount - 1; i >= 0; i--) {
        Local *local = &compiler->locals[i];
        if (identifiersEqual(name, &local->name)) { /* Checks if identifier matches */
            if (local->depth == UNDEFINED_SCOPE)
                compilerError(programCompiler, &programCompiler->parser->previous, RED_INIT_ERR);
            return i;
        }
    }

    return UNRESOLVED_LOCAL;
}

/**
 * Adds a new upvalue to the upvalue list and returns the index of the created upvalue.
 */
static int addUpvalue(ProgramCompiler *programCompiler, FunctionCompiler *compiler, uint8_t index,
                      bool isLocal) {
    int upvalueCount = compiler->function->upvalueCount;

    /* Checks if the upvalue is already in the upvalue list */
    for (int i = 0; i < upvalueCount; i++) {
        Upvalue *upvalue = &compiler->upvalues[i];
        if (upvalue->index == index && upvalue->isLocal == isLocal) return i;
    }

    if (upvalueCount == MAX_SINGLE_BYTE) {
        compilerError(programCompiler, &programCompiler->parser->previous, CLOSURE_LIMIT_ERR);
        return UNRESOLVED_LOCAL;
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
static int resolveUpvalue(ProgramCompiler *programCompiler, FunctionCompiler *compiler,
                          Token *name) {
    if (compiler->enclosing == NULL) /* Global variable? */
        return UNRESOLVED_LOCAL;

    /* Looks for a local variable in the enclosing scope */
    int local = resolveLocal(programCompiler, compiler->enclosing, name);
    if (local != UNRESOLVED_LOCAL) {
        compiler->enclosing->locals[local].isCaptured = true;
        return addUpvalue(programCompiler, compiler, (uint8_t) local, true);
    }

    /* Looks for an upvalue in the enclosing scope */
    int upvalue = resolveUpvalue(programCompiler, compiler->enclosing, name);
    if (upvalue != UNRESOLVED_LOCAL)
        return addUpvalue(programCompiler, compiler, (uint8_t) upvalue, false);

    return UNRESOLVED_LOCAL;
}

/**
 * Adds a local variable to the list of variables in a scope depth.
 */
static void addLocal(ProgramCompiler *compiler, Token name) {
    if (functionCompiler->localCount == MAX_SINGLE_BYTE) {
        compilerError(compiler, &compiler->parser->previous, VAR_LIMIT_ERR);
        return;
    }

    Local *local = &functionCompiler->locals[functionCompiler->localCount++];
    local->name = name;
    local->depth = UNDEFINED_SCOPE;
    local->isCaptured = false;
}

/**
 * Records the existence of a variable declaration.
 */
static void declareVariable(ProgramCompiler *compiler) {
    if (functionCompiler->scopeDepth == GLOBAL_SCOPE) return; /* Globals are late bound, exit */
    Token *name = &compiler->parser->previous;

    /* Verifies if local variable was previously declared */
    for (int i = functionCompiler->localCount - 1; i >= 0; i--) {
        Local *local = &functionCompiler->locals[i];
        if (local->depth != UNDEFINED_SCOPE && local->depth < functionCompiler->scopeDepth) break;
        if (identifiersEqual(name, &local->name)) /* Checks if already declared */
            compilerError(compiler, &compiler->parser->previous, VAR_REDECL_ERR);
    }

    addLocal(compiler, *name);
}

/**
 * Parses a variable by consuming an identifier token and then adds the the token lexeme to the
 * constants table.
 */
static uint8_t parseVariable(ProgramCompiler *compiler, const char *errorMessage) {
    consume(compiler, TK_IDENTIFIER, errorMessage);
    declareVariable(compiler); /* Declares the variables */

    if (functionCompiler->scopeDepth > GLOBAL_SCOPE)
        return ERROR_STATE; /* Locals are not looked up by name, exit */

    return identifierConstant(compiler, &compiler->parser->previous);
}

/**
 * Marks a local variable as initialized and available for use.
 */
static void markInitialized() {
    if (functionCompiler->scopeDepth == GLOBAL_SCOPE) return;
    functionCompiler->locals[functionCompiler->localCount - 1].depth = functionCompiler->scopeDepth;
}

/**
 * Handles a variable declaration by emitting the bytecode to perform a global variable declaration.
 */
static void defineVariable(Parser *parser, uint8_t global) {
    if (functionCompiler->scopeDepth > GLOBAL_SCOPE) {
        markInitialized(); /* Mark as initialized */
        return;            /* Only globals are defined at runtime */
    }

    emitBytes(parser, OP_DEFINE_GLOBAL, global);
}

/**
 * Compiles the list of arguments in a function call.
 */
static uint8_t argumentList(ProgramCompiler *compiler) {
    uint8_t argCount = 0;

    if (!check(compiler->parser, TK_RIGHT_PAREN)) {
        do {
            expression(compiler);
            if (argCount == UINT8_MAX)
                compilerError(compiler, &compiler->parser->previous, ARGS_LIMIT_ERR);
            argCount++;
        } while (match(compiler, TK_COMMA));
    }

    consume(compiler, TK_RIGHT_PAREN, CALL_LIST_PAREN_ERR);
    return argCount;
}

/**
 * Handles the "and" logical operator with short-circuit.
 */
static void and_(ProgramCompiler *compiler, bool canAssign) {
    int jump = emitJump(compiler->parser, OP_AND);
    parsePrecedence(compiler, PREC_AND);
    patchJump(compiler, jump);
}

/**
 * Handles the "or" logical operator with short-circuit.
 */
static void or_(ProgramCompiler *compiler, bool canAssign) {
    int jump = emitJump(compiler->parser, OP_OR);
    parsePrecedence(compiler, PREC_OR);
    patchJump(compiler, jump);
}

/**
 * Handles a binary (infix) expression, by compiling the right operand of the expression (the left
 * one was already compiled). Then, emits the bytecode instruction that performs the binary
 * operation.
 */
static void binary(ProgramCompiler *compiler, bool canAssign) {
    Parser *parser = compiler->parser;
    TokenType operatorType = parser->previous.type;
    ParseRule *rule = getRule(operatorType); /* Gets the current rule */
    parsePrecedence(compiler, rule->precedence + 1); /* Compiles with the correct precedence */

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
static void call(ProgramCompiler *compiler, bool canAssign) {
    uint8_t argCount = argumentList(compiler);
    emitBytes(compiler->parser, OP_CALL, argCount);
}

/**
 * Handles the opening parenthesis by compiling the expression between the parentheses, and then
 * parsing the closing parenthesis.
 */
static void grouping(ProgramCompiler *compiler, bool canAssign) {
    expression(compiler);
    consume(compiler, TK_RIGHT_PAREN, GRP_EXPR_ERR);
}

/**
 * Handles a literal (booleans or null) expression by outputting the proper instruction.
 */
static void literal(ProgramCompiler *compiler, bool canAssign) {
    Parser *parser = compiler->parser;
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
 * Handles a numeric expression by converting a string to a double number and then generates the
 * code to load that value by calling "emitConstant".
 */
static void number(ProgramCompiler *compiler, bool canAssign) {
    double value = strtod(compiler->parser->previous.start, NULL);
    emitConstant(compiler, NUM_VAL(value));
}

/**
 * Handles a exponentiation expression.
 */
static void pow_(ProgramCompiler *compiler, bool canAssign) {
    parsePrecedence(compiler, PREC_POW); /* Compiles the operand */
    emitByte(compiler->parser, OP_POW);
}

/**
 * Handles a string expression by creating a string object, wrapping it in a Value, and then
 * adding it to the constants table.
 */
static void string(ProgramCompiler *compiler, bool canAssign) {
    Parser *parser = compiler->parser;
    emitConstant(compiler, OBJ_VAL(copyString(compiler->vm, parser->previous.start + 1,
                                              parser->previous.length - 2)));
}

/**
 * Emits the instructions for a given compound assignment, which provide a shorter syntax for
 * assigning the result of an arithmetic operator.
 */
static void compoundAssignment(ProgramCompiler *compiler, uint8_t getOpcode, uint8_t setOpcode,
                               uint8_t arg, uint8_t opcode) {
    Parser *parser = compiler->parser;
    emitBytes(parser, getOpcode, (uint8_t) arg);
    expression(compiler);
    emitByte(parser, opcode);
    emitBytes(parser, setOpcode, (uint8_t) arg);
}

/**
 * Gets the index of a variable in the constants table and emits the the bytecode to perform the
 * load of the global/local variable.
 */
static void namedVariable(ProgramCompiler *compiler, Token name, bool canAssign) {
    uint8_t getOpcode, setOpcode;
    int arg = resolveLocal(compiler, functionCompiler, &name);

    /* Finds the current scope */
    if (arg != UNRESOLVED_LOCAL) { /* Local variable? */
        getOpcode = OP_GET_LOCAL;
        setOpcode = OP_SET_LOCAL;
    } else if ((arg = resolveUpvalue(compiler, functionCompiler, &name)) != -1) { /* Upvalue? */
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
        emitBytes(compiler->parser, setOpcode, (uint8_t) arg);
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
    } else {  /* Access variable */
        emitBytes(compiler->parser, getOpcode, (uint8_t) arg);
    }
}

/**
 * Handles a variable access.
 */
static void variable(ProgramCompiler *compiler, bool canAssign) {
    namedVariable(compiler, compiler->parser->previous, canAssign);
}

/**
 * Handles the ternary "?:" conditional operator expression.
 */
static void ternary(ProgramCompiler *compiler, bool canAssign) {
    Parser *parser = compiler->parser;

    int ifJump = emitJump(compiler->parser, OP_JUMP_IF_FALSE); /* Jumps if the condition is false */
    emitByte(parser, OP_POP);                                  /* Pops the condition result */
    parsePrecedence(compiler, PREC_TERNARY);                   /* Compiles the first branch */
    consume(compiler, TK_COLON, TERNARY_EXPR_ERR);

    int elseJump = emitJump(parser, OP_JUMP); /* Jumps the second branch if first was taken */
    patchJump(compiler, ifJump);              /* Patches the jump over the first branch */
    emitByte(parser, OP_POP);                 /* Pops the condition result */
    parsePrecedence(compiler, PREC_ASSIGN);   /* Compiles the second branch */
    patchJump(compiler, elseJump);            /* Patches the jump over the second branch */
}

/**
 * Handles a unary expression by compiling the operand and then emitting the bytecode to perform
 * the unary operation itself.
 */
static void unary(ProgramCompiler *compiler, bool canAssign) {
    Parser *parser = compiler->parser;
    TokenType operatorType = parser->previous.type;
    parsePrecedence(compiler, PREC_UNARY); /* Compiles the operand */

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
    EMPTY_RULE,                         /* TK_COLON */
    EMPTY_RULE,                         /* TK_SEMICOLON */
    EMPTY_RULE,                         /* TK_ARROW */
    RULE(unary, binary, PREC_TERM),     /* TK_MINUS */
    EMPTY_RULE,                         /* TK_MINUS_EQUAL */
    INFIX_RULE(binary, PREC_TERM),      /* TK_PLUS */
    EMPTY_RULE,                         /* TK_PLUS_EQUAL */
    INFIX_RULE(binary, PREC_FACTOR),    /* TK_DIV */
    EMPTY_RULE,                         /* TK_DIV_EQUAL */
    INFIX_RULE(binary, PREC_FACTOR),    /* TK_MOD */
    EMPTY_RULE,                         /* TK_MOD_EQUAL */
    INFIX_RULE(binary, PREC_FACTOR),    /* TK_MULTIPLY */
    EMPTY_RULE,                         /* TK_MULTIPLY_EQUAL */
    INFIX_RULE(pow_, PREC_POW),         /* TK_POW */
    EMPTY_RULE,                         /* TK_POW_EQUAL */
    PREFIX_RULE(unary),                 /* TK_NOT */
    INFIX_RULE(binary, PREC_EQUAL),     /* TK_NOT_EQUAL */
    EMPTY_RULE,                         /* TK_EQUAL */
    INFIX_RULE(binary, PREC_EQUAL),     /* TK_EQUAL_EQUAL */
    INFIX_RULE(binary, PREC_COMPARE),   /* TK_GREATER */
    INFIX_RULE(binary, PREC_COMPARE),   /* TK_GREATER_EQUAL */
    INFIX_RULE(binary, PREC_COMPARE),   /* TK_LESS */
    INFIX_RULE(binary, PREC_COMPARE),   /* TK_LESS_EQUAL */
    INFIX_RULE(and_, PREC_AND),         /* TK_AND */
    INFIX_RULE(or_, PREC_OR),           /* TK_OR */
    INFIX_RULE(ternary, PREC_TERNARY),  /* TK_TERNARY */
    PREFIX_RULE(variable),              /* TK_IDENTIFIER */
    PREFIX_RULE(string),                /* TK_STRING */
    PREFIX_RULE(number),                /* TK_NUMBER */
    EMPTY_RULE,                         /* TK_CLASS */
    EMPTY_RULE,                         /* TK_ELSE */
    PREFIX_RULE(literal),               /* TK_FALSE */
    EMPTY_RULE,                         /* TK_FOR */
    EMPTY_RULE,                         /* TK_FUNCTION */
    EMPTY_RULE,                         /* TK_IF */
    EMPTY_RULE,                         /* TK_NEXT */
    PREFIX_RULE(literal),               /* TK_NULL */
    EMPTY_RULE,                         /* TK_RETURN */
    EMPTY_RULE,                         /* TK_SUPER */
    EMPTY_RULE,                         /* TK_SWITCH */
    EMPTY_RULE,                         /* TK_THIS */
    PREFIX_RULE(literal),               /* TK_TRUE */
    EMPTY_RULE,                         /* TK_VAR */
    EMPTY_RULE,                         /* TK_WHEN */
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
static void parsePrecedence(ProgramCompiler *compiler, Precedence precedence) {
    advance(compiler);
    Parser *parser = compiler->parser;
    ParseFunction prefixRule = getRule(parser->previous.type)->prefix;

    if (prefixRule == NULL) { /* Checks if is a parsing error */
        compilerError(compiler, &parser->previous, EXPR_ERR);
        return;
    }

    /* Checks if the left side is assignable */
    bool canAssign = precedence <= PREC_TERNARY;
    prefixRule(compiler, canAssign);

    /* Looks for an infix parser for the next token */
    while (precedence <= getRule(parser->current.type)->precedence) {
        advance(compiler);
        ParseFunction infixRule = getRule(parser->previous.type)->infix;
        infixRule(compiler, canAssign);
    }

    if (canAssign && match(compiler, TK_EQUAL)) { /* Checks if is an invalid assignment */
        compilerError(compiler, &parser->previous, INV_ASSG_ERR);
        expression(compiler);
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
void expression(ProgramCompiler *compiler) { parsePrecedence(compiler, PREC_ASSIGN); }

/**
 * Compiles a block of code by parsing declarations and statements until a closing brace (end of
 * block) is found.
 */
static void block(ProgramCompiler *compiler) {
    while (!check(compiler->parser, TK_RIGHT_BRACE) && !check(compiler->parser, TK_EOF)) {
        declaration(compiler);
    }

    consume(compiler, TK_RIGHT_BRACE, BLOCK_BRACE_ERR);
}

/**
 * Compiles the declaration of a single variable.
 */
static void singleVarDeclaration(ProgramCompiler *compiler) {
    Parser *parser = compiler->parser;
    uint8_t global = parseVariable(compiler, VAR_NAME_ERR); /* Parses a variable name */

    if (match(compiler, TK_EQUAL)) {
        expression(compiler); /* Compiles the variable initializer */
    } else {
        emitByte(parser, OP_NULL); /* Default variable value is "null" */
    }

    defineVariable(parser, global); /* Emits the declaration bytecode */
}

/**
 * Compiles a variable declaration list.
 */
static void varDeclaration(ProgramCompiler *compiler) {
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
static void function(ProgramCompiler *programCompiler, FunctionType type) {
    Parser *parser = programCompiler->parser;
    FunctionCompiler compiler;
    initCompiler(programCompiler, &compiler, type);
    beginScope();

    /* Compiles the parameter list */
    consume(programCompiler, TK_LEFT_PAREN, FUNC_NAME_PAREN_ERR);
    if (!check(parser, TK_RIGHT_PAREN)) {
        do {
            functionCompiler->function->arity++;
            if (functionCompiler->function->arity > UINT8_MAX)
                compilerError(programCompiler, &parser->current, PARAMS_LIMIT_ERR);
            uint8_t paramConstant = parseVariable(programCompiler, PARAM_NAME_ERR);
            defineVariable(parser, paramConstant);
        } while (match(programCompiler, TK_COMMA));
    }
    consume(programCompiler, TK_RIGHT_PAREN, FUNC_LIST_PAREN_ERR);

    /* Compiles the function body */
    consume(programCompiler, TK_LEFT_BRACE, FUNC_BODY_BRACE_ERR);
    block(programCompiler);

    /* Create the function object */
    ObjFunction *function = endCompiler(parser);
    emitBytes(parser, OP_CLOSURE, makeConstant(programCompiler, OBJ_VAL(function)));

    /* Emits the captured upvalues */
    for (int i = 0; i < function->upvalueCount; i++) {
        emitByte(parser, (uint8_t) (compiler.upvalues[i].isLocal ? true : false));
        emitByte(parser, compiler.upvalues[i].index);
    }
}

/**
 * Compiles a function declaration.
 */
static void funDeclaration(ProgramCompiler *compiler) {
    uint8_t func = parseVariable(compiler, FUNC_NAME_ERR);
    markInitialized();
    function(compiler, TYPE_FUNCTION);
    defineVariable(compiler->parser, func);
}

/**
 * Compiles an expression statement.
 */
static void expressionStatement(ProgramCompiler *compiler) {
    expression(compiler);
    consume(compiler, TK_SEMICOLON, EXPR_STMT_ERR);
//    emitByte(compiler->parser, (compiler->vm->isREPL) ? OP_POP_EXPR : OP_POP);
    emitByte(compiler->parser, OP_POP);
}

/**
 * Compiles an "if" conditional statement.
 */
static void ifStatement(ProgramCompiler *compiler) {
    Parser *parser = compiler->parser;
    expression(compiler); /* Compiles condition */
    consume(compiler, TK_LEFT_BRACE, IF_STMT_ERR);

    int thenJump = emitJump(parser, OP_JUMP_IF_FALSE);
    emitByte(parser, OP_POP);

    /* Compiles the "if" block */
    beginScope();
    block(compiler);
    endScope(parser);

    int elseJump = emitJump(parser, OP_JUMP);
    patchJump(compiler, thenJump);
    emitByte(parser, OP_POP);

    /* Compiles the "else" block */
    if (match(compiler, TK_ELSE)) {
        if (match(compiler, TK_IF)) {
            ifStatement(compiler); /* "else if ..." form */
        } else if (match(compiler, TK_LEFT_BRACE)) {
            beginScope();
            block(compiler); /* Compiles the "else" branch */
            endScope(parser);
        }
    }

    patchJump(compiler, elseJump);
}

/**
 * Compiles a "switch" conditional statement.
 */
static void switchStatement(ProgramCompiler *compiler) {
    Parser *parser = compiler->parser;

    /* Possible switch states */
    typedef enum {
        BEF_CASES, /* Before all cases */
        BEF_ELSE,  /* Before else case */
        AFT_ELSE   /* After else case */
    } SwitchState;

    SwitchState switchState = BEF_CASES;
    int caseEnds[MAX_SINGLE_BYTE];
    int caseCount = 0;
    int previousCaseSkip = -1;

    expression(compiler); /* Compiles expression to switch on */
    consume(compiler, TK_LEFT_BRACE, SWITCH_STMT_ERR);

    while (!match(compiler, TK_RIGHT_BRACE) && !check(parser, TK_EOF)) {
        if (match(compiler, TK_WHEN) || match(compiler, TK_ELSE)) {
            TokenType caseType = parser->previous.type;

            if (switchState == AFT_ELSE) { /* Already compiled the else case? */
                compilerError(compiler, &parser->previous, ELSE_END_ERR);
            } else if (switchState == BEF_ELSE) {                  /* Else case not compiled yet? */
                caseEnds[caseCount++] = emitJump(parser, OP_JUMP); /* Jumps over the other cases */
                patchJump(compiler, previousCaseSkip);             /* Patches the jump */
                emitByte(parser, OP_POP);
            }

            if (caseType == TK_WHEN) {
                switchState = BEF_ELSE;

                /* Checks if the case is equal to the switch value */
                emitByte(parser, OP_DUP); /* "==" pops its operand, so duplicate before */
                expression(compiler);
                consume(compiler, TK_ARROW, ARR_CASE_ERR);
                emitByte(parser, OP_EQUAL);
                previousCaseSkip = emitJump(parser, OP_JUMP_IF_FALSE);

                emitByte(parser, OP_POP); /* Pops the comparison result */
            } else {
                switchState = AFT_ELSE;
                consume(compiler, TK_ARROW, ARR_ELSE_ERR);
                previousCaseSkip = -1;
            }
        } else {
            if (switchState == BEF_CASES) /* Statement outside a case? */
                compilerError(compiler, &parser->previous, STMT_SWITCH_ERR);
            statement(compiler); /* Statement is inside a case */
        }
    }

    /* If no else case, patch its condition jump */
    if (switchState == BEF_ELSE) {
        patchJump(compiler, previousCaseSkip);
        emitByte(parser, OP_POP);
    }

    /* Patch all the case jumps to the end */
    for (int i = 0; i < caseCount; i++) {
        patchJump(compiler, caseEnds[i]);
    }

    emitByte(parser, OP_POP); /* Pops the switch value */
}

/**
 * Compiles a "while" loop statement.
 */
static void whileStatement(ProgramCompiler *compiler) {
    Parser *parser = compiler->parser;

    /* Loop's control flow marks */
    int surroundingLoopStart = parser->innermostLoopStart;
    int surroundingLoopScopeDepth = parser->innermostLoopScopeDepth;
    parser->innermostLoopStart = currentBytecodeChunk()->count;
    parser->innermostLoopScopeDepth = functionCompiler->scopeDepth;

    expression(compiler); /* Compiles the loop condition */
    consume(compiler, TK_LEFT_BRACE, WHILE_STMT_ERR);

    int exitJump = emitJump(parser, OP_JUMP_IF_FALSE);
    emitByte(parser, OP_POP);

    /* Compiles the "while" block */
    beginScope();
    block(compiler);
    endScope(parser);

    /* Emits the loop and patches the next jump */
    emitLoop(compiler, parser->innermostLoopStart);
    patchJump(compiler, exitJump);
    emitByte(parser, OP_POP);

    parser->innermostLoopStart = surroundingLoopStart;
    parser->innermostLoopScopeDepth = surroundingLoopScopeDepth;
}

/**
 * Compiles a "for" loop statement.
 */
static void forStatement(ProgramCompiler *compiler) {
    Parser *parser = compiler->parser;
    beginScope(); /* Begins the loop scope */

    /* Compiles the initializer clause */
    if (match(compiler, TK_COMMA)) {
        compilerError(compiler, &parser->previous, FOR_STMT_INIT_ERR); /* Empty initializer */
    } else {
        singleVarDeclaration(compiler); /* Variable declaration initializer */
        consume(compiler, TK_COMMA, FOR_STMT_CM1_ERR);
    }

    /* Loop's control flow marks */
    int surroundingLoopStart = parser->innermostLoopStart;
    int surroundingLoopScopeDepth = parser->innermostLoopScopeDepth;
    parser->innermostLoopStart = currentBytecodeChunk()->count;
    parser->innermostLoopScopeDepth = functionCompiler->scopeDepth;

    /* Compiles the conditional clause */
    expression(compiler);
    consume(compiler, TK_COMMA, FOR_STMT_CM2_ERR);
    int exitJump = emitJump(parser, OP_JUMP_IF_FALSE);
    emitByte(parser, OP_POP); /* Pops condition */

    /* Compiles the increment clause */
    int bodyJump = emitJump(parser, OP_JUMP);
    int incrementStart = currentBytecodeChunk()->count;
    expression(compiler);
    emitByte(parser, OP_POP); /* Pops increment */
    consume(compiler, TK_LEFT_BRACE, FOR_STMT_BRC_ERR);
    emitLoop(compiler, parser->innermostLoopStart);
    parser->innermostLoopStart = incrementStart;
    patchJump(compiler, bodyJump);

    block(compiler); /* Compiles the "for" block */
    emitLoop(compiler, parser->innermostLoopStart);
    if (exitJump != -1) {
        patchJump(compiler, exitJump);
        emitByte(parser, OP_POP); /* Pops condition */
    }

    parser->innermostLoopStart = surroundingLoopStart;
    parser->innermostLoopScopeDepth = surroundingLoopScopeDepth;
    endScope(parser); /* Ends the loop scope */
}

/**
 * Compiles a "next" control flow statement, advancing to the next iteration of a loop and
 * discarding locals created in the loop.
 */
static void nextStatement(ProgramCompiler *compiler) {
    Parser *parser = compiler->parser;
    if (parser->innermostLoopStart == NO_LOOP) /* Is outside of a loop body? */
        compilerError(compiler, &parser->previous, NEXT_LOOP_ERR);
    consume(compiler, TK_SEMICOLON, NEXT_STMT_ERR);

    /* Discards locals created in loop */
    for (int i = functionCompiler->localCount - 1;
         i >= 0 && functionCompiler->locals[i].depth > parser->innermostLoopScopeDepth; i--) {
        emitByte(parser, OP_POP);
    }

    emitLoop(compiler, parser->innermostLoopStart); /* Jumps to top of current innermost loop */
}

/**
 * Compiles a "return" statement.
 */
static void returnStatement(ProgramCompiler *compiler) {
    Parser *parser = compiler->parser;
    if (functionCompiler->type == TYPE_SCRIPT) /* Checks if in top level code */
        compilerError(compiler, &parser->previous, RETURN_TOP_LEVEL_ERR);

    if (match(compiler, TK_SEMICOLON)) {
        emitReturn(parser);
    } else {
        expression(compiler);
        consume(compiler, TK_SEMICOLON, RETURN_STMT_ERR);
        emitByte(parser, OP_RETURN);
    }
}

/**
 * Compiles a statement.
 */
static void statement(ProgramCompiler *compiler) {
    if (match(compiler, TK_IF)) {
        ifStatement(compiler);
    } else if(match(compiler, TK_SWITCH)) {
        switchStatement(compiler);
    } else if (match(compiler, TK_WHILE)) {
        whileStatement(compiler);
    } else if (match(compiler, TK_FOR)) {
        forStatement(compiler);
    } else if (match(compiler, TK_NEXT)) {
        nextStatement(compiler);
    } else if (match(compiler, TK_RETURN)) {
        returnStatement(compiler);
    } else if (match(compiler, TK_LEFT_BRACE)) {
        beginScope();
        block(compiler);
        endScope(compiler->parser);
    } else {
        expressionStatement(compiler);
    }
}

/**
 * Synchronize error recovery.
 */
static void synchronize(ProgramCompiler *compiler) {
    Parser *parser = compiler->parser;
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

        advance(compiler);
    }
}

/**
 * Compiles a declaration statement.
 */
static void declaration(ProgramCompiler *compiler) {
    if (match(compiler, TK_VAR)) {
        varDeclaration(compiler);
        consume(compiler, TK_SEMICOLON, VAR_DECL_ERR);
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
ObjFunction *compile(VM *vm, const char *source) {
    Parser parser;
    Scanner scanner;
    ProgramCompiler programCompiler;
    FunctionCompiler funCompiler;

    /* Inits the parser, scanner, and compiler */
    initParser(&parser);
    initScanner(source, &scanner);
    initProgramCompiler(&programCompiler, vm, &parser, &scanner);
    initCompiler(&programCompiler, &funCompiler, TYPE_SCRIPT);

    advance(&programCompiler);                 /* Get the first token */
    while (!match(&programCompiler, TK_EOF)) { /* Main compiler loop */
        declaration(&programCompiler);         /* YAPL's grammar entry point */
    }

    ObjFunction *function = endCompiler(programCompiler.parser);
    return programCompiler.parser->hadError ? NULL : function;
}
