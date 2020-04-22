/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * fl-compiler.c: Falcons's handwritten Parser/Compiler based on Pratt Parsing
 * See Falcon's license in the LICENSE file
 */

#include "fl-compiler.h"
#include "../lib/fl-strlib.h"
#include "fl-debug.h"
#include <stdio.h>
#include <string.h>

/* Precedence levels, from lowest to highest */
typedef enum {
    PREC_NONE,    /* No precedence */
    PREC_ASSIGN,  /* 1: "=" */
    PREC_TERNARY, /* 2: "?:" */
    PREC_OR,      /* 3: "or" */
    PREC_AND,     /* 4: "and" */
    PREC_EQUAL,   /* 5: "==", "!=" */
    PREC_COMPARE, /* 6: "<", ">", "<=", ">=" */
    PREC_TERM,    /* 7: "+", "-" */
    PREC_FACTOR,  /* 8: "*", "/", "%" */
    PREC_UNARY,   /* 9: "not", "-" */
    PREC_POW,     /* 10: "^" */
    PREC_TOP      /* Highest precedence: calls, subscripts, and fields get/set */
} PrecedenceLevels;

/* Function pointer to the parsing functions */
typedef void (*ParseFunction)(FalconCompiler *compiler, bool canAssign);

/* Parsing rules (prefix function, infix function, precedence level) */
typedef struct {
    ParseFunction prefix;
    ParseFunction infix;
    PrecedenceLevels precedence;
} ParseRule;

/**
 * Prints to stderr a compile-time error. The error message consists of a given message and some
 * debugging information, such as line and column numbers of where the error occurred.
 */
static void compile_time_error(FalconVM *vm, Scanner *scanner, Token *token, const char *message) {
    uint32_t tkLine = token->line;
    uint32_t tkColumn = token->column;
    const int offset = (token->type == TK_EOF) ? 1 : 0;
    const char *fileName = vm->fileName;
    const char *sourceLine = get_current_line(scanner);

    /* Prints the file, the line, the error message, and the source line */
    fprintf(stderr, "%s:%d:%d => ", fileName, tkLine, tkColumn);
    fprintf(stderr, "CompilerError: %s\n", message);
    fprintf(stderr, "%d | ", tkLine);

    for (int i = 0; sourceLine[i] != '\0'; i++) {
        if (sourceLine[i] == '\n') break;
        fprintf(stderr, "%c", sourceLine[i]);
    }

    /* Prints error indicator */
    fprintf(stderr, "\n");
    fprintf(stderr, "%*c^\n", tkColumn + 3 + offset, ' ');
}

/**
 * Prints to stderr a syntax/compiler error and sets the parser to go on panic mode.
 */
static void compiler_error(FalconCompiler *compiler, Token *token, const char *message) {
    if (compiler->parser->panicMode) return; /* Checks and sets error recovery */
    compiler->parser->panicMode = true;
    compile_time_error(compiler->vm, compiler->scanner, token, message); /* Presents the error */
    compiler->parser->hadError = true;
}

/**
 * Initializes a given Parser instance as error-free and with no loops.
 */
static void init_parser(Parser *parser) {
    parser->hadError = false;
    parser->panicMode = false;
}

/**
 * Takes the old current token and then loops through the token stream to get the next token. The
 * loop keeps going reading tokens and reporting the errors, until it hits a non-error one or
 * reaches EOF.
 */
static void advance(FalconCompiler *compiler) {
    compiler->parser->previous = compiler->parser->current;

    while (true) {
        compiler->parser->current = scan_token(compiler->scanner, compiler->vm);
        if (compiler->parser->current.type != TK_ERROR) break;
        compiler_error(compiler, &compiler->parser->current, compiler->parser->current.start);
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

    compiler_error(compiler, &compiler->parser->current, message);
}

/**
 * Checks if the current token if of the given type.
 */
static bool check(Parser *parser, FalconTokens type) { return parser->current.type == type; }

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
static ObjFunction *current_function(FunctionCompiler *fCompiler) { return fCompiler->function; }

/**
 * Returns the compiling bytecode chunk.
 */
static BytecodeChunk *current_bytecode(FunctionCompiler *fCompiler) {
    return &current_function(fCompiler)->bytecode;
}

/**
 * Appends a single byte to the bytecode chunk.
 */
static void emit_byte(FalconCompiler *compiler, uint8_t byte) {
    write_bytecode(compiler->vm, current_bytecode(compiler->fCompiler), byte,
                   compiler->parser->previous.line);
}

/**
 * Appends two bytes (operation code and operand) to the bytecode chunk.
 */
static void emit_bytes(FalconCompiler *compiler, uint8_t byte_1, uint8_t byte_2) {
    emit_byte(compiler, byte_1);
    emit_byte(compiler, byte_2);
}

/**
 * Emits a new "loop back" instruction which jumps backwards by a given offset.
 */
static void emit_loop(FalconCompiler *compiler, int loopStart) {
    emit_byte(compiler, OP_LOOP);
    uint16_t offset = (uint16_t)(current_bytecode(compiler->fCompiler)->count - loopStart + 2);

    if (offset > UINT16_MAX) /* Loop is too long? */
        compiler_error(compiler, &compiler->parser->previous, COMP_LOOP_LIMIT_ERR);

    emit_byte(compiler, (uint8_t)((uint16_t)(offset >> 8u) & 0xffu));
    emit_byte(compiler, (uint8_t)(offset & 0xffu));
}

/**
 * Emits the bytecode of a given instruction and reserve two bytes for a jump offset.
 */
static int emit_jump(FalconCompiler *compiler, uint8_t instruction) {
    emit_byte(compiler, instruction);
    emit_byte(compiler, 0xff);
    emit_byte(compiler, 0xff);
    return current_bytecode(compiler->fCompiler)->count - 2;
}

/**
 * Emits the "OP_RETURN" bytecode instruction. This function is called when a "return" statement
 * does not have a return value or when there is no "return" statement at all.
 */
static void emit_return(FalconCompiler *compiler) {
    if (compiler->fCompiler->type == TYPE_INIT) {
        emit_bytes(compiler, OP_GETLOCAL, 0); /* Emits the instance if in a class */
    } else {
        emit_byte(compiler, OP_LOADNULL); /* Emits the "null" default value */
    }

    emit_byte(compiler, OP_RETURN); /* Returns the value emitted before */
}

/**
 * Emits the bytecode for the definition of a given collection type (lists or maps).
 */
static void emit_collection(FalconCompiler *compiler, uint8_t opcode, uint16_t elementsCount) {
    emit_byte(compiler, opcode);
    emit_byte(compiler, (uint8_t)((uint64_t)(elementsCount >> 8u) & 0xffu));
    emit_byte(compiler, (uint8_t)(elementsCount & 0xffu));
}

/**
 * Adds a constant to the bytecode chunk constants table.
 */
static uint8_t make_constant(FalconCompiler *compiler, FalconValue value) {
    int constant = add_constant(compiler->vm, current_bytecode(compiler->fCompiler), value);
    if (constant > UINT8_MAX) { /* TODO: update that limit */
        compiler_error(compiler, &compiler->parser->previous, COMP_CONST_LIMIT_ERR);
        return 0;
    }

    return (uint8_t) constant;
}

/**
 * Writes a to a bytecode chunk the "OP_LOADCONST" instruction with a 16 bits argument.
 */
static void write_constant(FalconVM *vm, BytecodeChunk *bytecode, uint16_t index, int line) {
    write_bytecode(vm, bytecode, OP_LOADCONST, line);
    write_bytecode(vm, bytecode, (uint8_t)(index & 0xffu), line);
    write_bytecode(vm, bytecode, (uint8_t)((uint16_t)(index >> 8u) & 0xffu), line);
}

/**
 * Inserts a constant in the constants table of the current bytecode chunk. Before inserting,
 * checks if the constant limit was exceeded.
 */
static void emit_constant(FalconCompiler *compiler, FalconValue value) {
    int constant = add_constant(compiler->vm, current_bytecode(compiler->fCompiler), value);
    if (constant > UINT16_MAX) {
        compiler_error(compiler, &compiler->parser->previous, COMP_CONST_LIMIT_ERR);
    } else {
        write_constant(compiler->vm, current_bytecode(compiler->fCompiler), (uint16_t) constant,
                       compiler->parser->previous.line);
    }
}

/**
 * Replaces the operand at the given location with the calculated jump offset. This function
 * should be called right before the emission of the next instruction that the jump should
 * land on.
 */
static void patch_jump(FalconCompiler *compiler, int offset) {
    uint16_t jump = (uint16_t)(current_bytecode(compiler->fCompiler)->count - offset -
                               2); /* -2 to adjust by offset */

    if (jump > UINT16_MAX) /* Jump is too long? */
        compiler_error(compiler, &compiler->parser->previous, COMP_JUMP_LIMIT_ERR);

    current_bytecode(compiler->fCompiler)->code[offset] = (uint8_t)((uint16_t)(jump >> 8u) & 0xffu);
    current_bytecode(compiler->fCompiler)->code[offset + 1] = (uint8_t)(jump & 0xffu);
}

/**
 * Starts a new program compiler.
 */
static void init_compiler(FalconCompiler *compiler, FalconVM *vm, Parser *parser,
                          Scanner *scanner) {
    compiler->vm = vm;
    compiler->parser = parser;
    compiler->scanner = scanner;
}

/**
 * Starts a new function compilation process.
 */
static void init_function_compiler(FalconCompiler *compiler, FunctionCompiler *fCompiler,
                                   FunctionCompiler *enclosing, FunctionType type) {
    fCompiler->enclosing = enclosing;
    fCompiler->function = NULL;
    fCompiler->loop = NULL;
    fCompiler->type = type;
    fCompiler->localCount = 0;
    fCompiler->scopeDepth = COMP_GLOBAL_SCOPE;
    fCompiler->function = new_ObjFunction(compiler->vm);
    compiler->vm->compiler = compiler->fCompiler = fCompiler;

    Parser *parser = compiler->parser;
    if (type != TYPE_SCRIPT)
        current_function(compiler->fCompiler)->name = new_ObjString(
            compiler->vm, parser->previous.start, parser->previous.length); /* Sets function name */

    /* Set stack slot zero for the VM's internal use */
    Local *local = &compiler->fCompiler->locals[compiler->fCompiler->localCount++];
    local->depth = COMP_GLOBAL_SCOPE;
    local->isCaptured = false;

    /* Set slot zero for "this" if a method is being compiled */
    if (type != TYPE_FUNCTION) {
        local->name.start = "this";
        local->name.length = 4;
    } else {
        local->name.start = "";
        local->name.length = 0;
    }
}

/**
 * Ends the function compilation process.
 */
static ObjFunction *end_function_compiler(FalconCompiler *compiler) {
    emit_return(compiler);
    ObjFunction *function = current_function(compiler->fCompiler);

    /* Dumps the function's bytecode if the option "-d" is set */
    if (compiler->vm->dumpOpcodes && !compiler->parser->hadError)
        dump_bytecode(compiler->vm, function);

    compiler->vm->compiler = compiler->fCompiler = compiler->fCompiler->enclosing;
    return function;
}

/**
 * Begins a new scope by incrementing the current scope depth.
 */
static void begin_scope(FunctionCompiler *fCompiler) { fCompiler->scopeDepth++; }

/**
 * Ends an existing scope by decrementing the current scope depth and popping all local variables
 * declared in the scope.
 */
static void end_scope(FalconCompiler *compiler) {
    FunctionCompiler *fCompiler = compiler->fCompiler;
    fCompiler->scopeDepth--;

    /* Closes locals and upvalues in the scope */
    while (fCompiler->localCount > 0 &&
           fCompiler->locals[fCompiler->localCount - 1].depth > fCompiler->scopeDepth) {
        if (fCompiler->locals[fCompiler->localCount - 1].isCaptured) {
            emit_byte(compiler, OP_CLOSEUPVAL);
        } else {
            emit_byte(compiler, OP_POPT);
        }

        fCompiler->localCount--;
    }
}

/* Forward parser's declarations, since the grammar is recursive */
static void expression(FalconCompiler *compiler);
static void statement(FalconCompiler *compiler);
static void declaration(FalconCompiler *compiler);
static ParseRule *get_parse_rule(FalconTokens type);
static void parse_precedence(FalconCompiler *compiler, PrecedenceLevels precedence);

/**
 * Checks if an identifier constant was already defined. If so, return its index. If not, set the
 * identifier in the global names table.
 */
static uint8_t identifier_constant(FalconCompiler *compiler, Token *name) {
    return make_constant(compiler, OBJ_VAL(new_ObjString(compiler->vm, name->start, name->length)));
}

/**
 * Checks if two identifiers match.
 */
static bool identifiers_equal(Token *a, Token *b) {
    if (a->length != b->length) return false;
    return memcmp(a->start, b->start, (size_t) a->length) == 0;
}

/**
 * Resolves a local variable by looping through the list of locals that are currently in the scope.
 * If one has the same name as the identifier token, the variable is resolved.
 */
static int resolve_local(FalconCompiler *compiler, FunctionCompiler *fCompiler, Token *name) {
    for (int i = fCompiler->localCount - 1; i >= 0; i--) {
        Local *local = &fCompiler->locals[i];
        if (identifiers_equal(name, &local->name)) { /* Checks if identifier matches */
            if (local->depth == COMP_UNDEF_SCOPE)
                compiler_error(compiler, &compiler->parser->previous, COMP_READ_INIT_ERR);
            return i;
        }
    }

    return COMP_UNRESOLVED_LOCAL;
}

/**
 * Adds a new upvalue to the upvalue list and returns the index of the created upvalue.
 */
static int add_upvalue(FalconCompiler *compiler, FunctionCompiler *fCompiler, uint8_t index,
                       bool isLocal) {
    ObjFunction *function = current_function(fCompiler);
    int upvalueCount = function->upvalueCount;

    /* Checks if the upvalue is already in the upvalue list */
    for (int i = 0; i < upvalueCount; i++) {
        Upvalue *upvalue = &fCompiler->upvalues[i];
        if (upvalue->index == index && upvalue->isLocal == isLocal) return i;
    }

    if (upvalueCount == FALCON_MAX_BYTE) { /* Exceeds the limit os upvalues? */
        compiler_error(compiler, &compiler->parser->previous, COMP_CLOSURE_LIMIT_ERR);
        return COMP_UNRESOLVED_LOCAL;
    }

    /* Adds the new upvalue */
    fCompiler->upvalues[upvalueCount].isLocal = isLocal;
    fCompiler->upvalues[upvalueCount].index = index;
    return function->upvalueCount++;
}

/**
 * Resolves an upvalue by looking for a local variable declared in any of the surrounding scopes.
 * If found, an upvalue index for that variable is returned.
 */
static int resolve_upvalue(FalconCompiler *compiler, FunctionCompiler *fCompiler, Token *name) {
    if (fCompiler->enclosing == NULL) /* Global variable? */
        return COMP_UNRESOLVED_LOCAL;

    /* Looks for a local variable in the enclosing scope */
    int local = resolve_local(compiler, fCompiler->enclosing, name);
    if (local != COMP_UNRESOLVED_LOCAL) {
        fCompiler->enclosing->locals[local].isCaptured = true;
        return add_upvalue(compiler, fCompiler, (uint8_t) local, true);
    }

    /* Looks for an upvalue in the enclosing scope */
    int upvalue = resolve_upvalue(compiler, fCompiler->enclosing, name);
    if (upvalue != COMP_UNRESOLVED_LOCAL)
        return add_upvalue(compiler, fCompiler, (uint8_t) upvalue, false);

    return COMP_UNRESOLVED_LOCAL;
}

/**
 * Adds a local variable to the list of variables in a scope depth.
 */
static void add_local(FalconCompiler *compiler, Token name) {
    FunctionCompiler *fCompiler = compiler->fCompiler;
    if (fCompiler->localCount == FALCON_MAX_BYTE) {
        compiler_error(compiler, &compiler->parser->previous, COMP_VAR_LIMIT_ERR);
        return;
    }

    Local *local = &fCompiler->locals[fCompiler->localCount++];
    local->name = name;
    local->depth = COMP_UNDEF_SCOPE;
    local->isCaptured = false;
}

/**
 * Records the existence of a variable declaration.
 */
static void declare_variable(FalconCompiler *compiler) {
    FunctionCompiler *fCompiler = compiler->fCompiler;
    if (fCompiler->scopeDepth == COMP_GLOBAL_SCOPE) return; /* Globals are late bound, exit */
    Token *name = &compiler->parser->previous;

    /* Verifies if local variable was previously declared */
    for (int i = fCompiler->localCount - 1; i >= 0; i--) {
        Local *local = &fCompiler->locals[i];
        if (local->depth != COMP_UNDEF_SCOPE && local->depth < fCompiler->scopeDepth) break;
        if (identifiers_equal(name, &local->name)) /* Checks if already declared */
            compiler_error(compiler, &compiler->parser->previous, COMP_VAR_REDECL_ERR);
    }

    add_local(compiler, *name);
}

/**
 * Parses a variable by consuming an identifier token and then adds the the token lexeme to the
 * constants table.
 */
static uint8_t parse_variable(FalconCompiler *compiler, const char *errorMessage) {
    consume(compiler, TK_IDENTIFIER, errorMessage);
    declare_variable(compiler); /* Declares the variables */

    if (compiler->fCompiler->scopeDepth > COMP_GLOBAL_SCOPE)
        return 0; /* Locals are not looked up by name, exit */

    return identifier_constant(compiler, &compiler->parser->previous);
}

/**
 * Marks a local variable as initialized and available for use.
 */
static void mark_as_initialized(FunctionCompiler *fCompiler) {
    if (fCompiler->scopeDepth == COMP_GLOBAL_SCOPE) return;
    fCompiler->locals[fCompiler->localCount - 1].depth = fCompiler->scopeDepth;
}

/**
 * Handles a variable declaration by emitting the bytecode to perform a global variable declaration.
 */
static void define_variable(FalconCompiler *compiler, uint8_t global) {
    if (compiler->fCompiler->scopeDepth > COMP_GLOBAL_SCOPE) {
        mark_as_initialized(compiler->fCompiler); /* Mark as initialized */
        return;                                   /* Only globals are defined at runtime */
    }

    emit_bytes(compiler, OP_DEFGLOBAL, global);
}

/**
 * Compiles the list of arguments in a function call.
 */
static uint8_t argument_list(FalconCompiler *compiler) {
    uint8_t argCount = 0;
    if (!check(compiler->parser, TK_RPAREN)) {
        do {
            expression(compiler);
            if (argCount == UINT8_MAX) /* Reached max args? */
                compiler_error(compiler, &compiler->parser->previous, COMP_ARGS_LIMIT_ERR);
            argCount++;
        } while (match(compiler, TK_COMMA));
    }

    consume(compiler, TK_RPAREN, COMP_CALL_LIST_PAREN_ERR);
    return argCount;
}

/**
 * Gets the index of a variable in the constants table and emits the the bytecode to perform the
 * load of the global/local variable.
 */
static void named_variable(FalconCompiler *compiler, Token name, bool canAssign) {
    uint8_t getOpcode, setOpcode;
    int arg = resolve_local(compiler, compiler->fCompiler, &name);

    /* Finds the current scope */
    if (arg != COMP_UNRESOLVED_LOCAL) { /* Local variable? */
        getOpcode = OP_GETLOCAL;
        setOpcode = OP_SETLOCAL;
    } else if ((arg = resolve_upvalue(compiler, compiler->fCompiler, &name)) != -1) { /* Upvalue? */
        getOpcode = OP_GETUPVAL;
        setOpcode = OP_SETUPVAL;
    } else { /* Global variable */
        arg = identifier_constant(compiler, &name);
        getOpcode = OP_GETGLOBAL;
        setOpcode = OP_SETGLOBAL;
    }

    /* Compiles variable assignments or access */
    if (canAssign && match(compiler, TK_EQUAL)) { /* a = ... */
        expression(compiler);
        emit_bytes(compiler, setOpcode, (uint8_t) arg);
    } else { /* Access variable */
        emit_bytes(compiler, getOpcode, (uint8_t) arg);
    }
}

/**
 * Creates a synthetic token (i.e., a token not listed on "compiler/falcon_tokens.h") for a given
 * constant string.
 * TODO: move to "fl-scanner"?
 */
static Token synthetic_token(const char *constant) {
    Token token;
    token.start = constant;
    token.length = (int) strlen(constant);
    return token;
}

/* Common interface to all parsing rules */
#define PARSE_RULE(name) static void name(FalconCompiler *compiler, bool canAssign)

/**
 * Handles the "and" logical operator with short-circuit.
 */
PARSE_RULE(and_) {
    (void) canAssign; /* Unused */
    int jump = emit_jump(compiler, OP_AND);
    parse_precedence(compiler, PREC_AND);
    patch_jump(compiler, jump);
}

/**
 * Handles the "or" logical operator with short-circuit.
 */
PARSE_RULE(or_) {
    (void) canAssign; /* Unused */
    int jump = emit_jump(compiler, OP_OR);
    parse_precedence(compiler, PREC_OR);
    patch_jump(compiler, jump);
}

/**
 * Handles a exponentiation expression.
 */
PARSE_RULE(pow_) {
    (void) canAssign;                     /* Unused */
    parse_precedence(compiler, PREC_POW); /* Compiles the operand */
    emit_byte(compiler, OP_POW);
}

/**
 * Handles a binary (infix) expression, by compiling the right operand of the expression (the left
 * one was already compiled). Then, emits the bytecode instruction that performs the binary
 * operation.
 */
PARSE_RULE(binary) {
    (void) canAssign; /* Unused */
    FalconTokens operatorType = compiler->parser->previous.type;
    ParseRule *rule = get_parse_rule(operatorType);   /* Gets the current rule */
    parse_precedence(compiler, rule->precedence + 1); /* Compiles with the correct precedence */

    /* Emits the operator instruction */
    switch (operatorType) {
        case TK_NOTEQUAL: emit_bytes(compiler, OP_EQUAL, OP_NOT); break;
        case TK_EQEQUAL: emit_byte(compiler, OP_EQUAL); break;
        case TK_GREATER: emit_byte(compiler, OP_GREATER); break;
        case TK_GREATEREQUAL: emit_bytes(compiler, OP_LESS, OP_NOT); break;
        case TK_LESS: emit_byte(compiler, OP_LESS); break;
        case TK_LESSEQUAL: emit_bytes(compiler, OP_GREATER, OP_NOT); break;
        case TK_PLUS: emit_byte(compiler, OP_ADD); break;
        case TK_MINUS: emit_byte(compiler, OP_SUB); break;
        case TK_SLASH: emit_byte(compiler, OP_DIV); break;
        case TK_PERCENT: emit_byte(compiler, OP_MOD); break;
        case TK_STAR: emit_byte(compiler, OP_MULT); break;
        default: return; /* Unreachable */
    }
}

/**
 * Handles a variable access.
 */
PARSE_RULE(variable) { named_variable(compiler, compiler->parser->previous, canAssign); }

/**
 * Handles a function call expression by parsing its arguments list and emitting the instruction to
 * proceed with the execution of the function.
 */
PARSE_RULE(call) {
    (void) canAssign; /* Unused */
    uint8_t argCount = argument_list(compiler);
    emit_bytes(compiler, OP_CALL, argCount);
}

/**
 * Handles the "dot" syntax for get and set expressions, and method calls, on a class instance. If
 * the expression is a direct method call, the more efficient "OP_INVPROP" instruction is emitted.
 */
PARSE_RULE(dot) {
    consume(compiler, TK_IDENTIFIER, COMP_PROP_NAME_ERR);
    uint8_t name = identifier_constant(compiler, &compiler->parser->previous);

    /* Compiles field get or set expressions, and method calls */
    if (canAssign && match(compiler, TK_EQUAL)) { /* a.b = ... */
        expression(compiler);
        emit_bytes(compiler, OP_SETPROP, name);
    } else if (match(compiler, TK_LPAREN)) { /* Method call */
        uint8_t argCount = argument_list(compiler);
        emit_bytes(compiler, OP_INVPROP, name);
        emit_byte(compiler, argCount);
    } else { /* Access field */
        emit_bytes(compiler, OP_GETPROP, name);
    }
}

/**
 * Handles the opening parenthesis by compiling the expression between the parentheses, and then
 * parsing the closing parenthesis.
 */
PARSE_RULE(grouping) {
    (void) canAssign; /* Unused */
    expression(compiler);
    consume(compiler, TK_RPAREN, COMP_GRP_EXPR_ERR);
}

/**
 * Handles a list literal by creating a new Falcon list and compiling each of its elements.
 */
PARSE_RULE(list) {
    (void) canAssign; /* Unused */
    uint16_t elementsCount = 0;

    /* Parses the list elements */
    if (!check(compiler->parser, TK_RBRACKET)) {
        do {
            expression(compiler);
            if (elementsCount == UINT16_MAX) /* Reached max elements? */
                compiler_error(compiler, &compiler->parser->previous, COMP_LIST_LIMIT_ERR);
            elementsCount++;
        } while (match(compiler, TK_COMMA));
    }

    consume(compiler, TK_RBRACKET, COMP_LIST_BRACKET_ERR);
    emit_collection(compiler, OP_DEFLIST, elementsCount); /* Creates the new list */
}

/**
 * Handles a literal (booleans or null) expression by outputting the proper instruction.
 */
PARSE_RULE(literal) {
    (void) canAssign; /* Unused */
    switch (compiler->parser->previous.type) {
        case TK_FALSE: emit_byte(compiler, OP_LOADFALSE); break;
        case TK_NULL: emit_byte(compiler, OP_LOADNULL); break;
        case TK_TRUE: emit_byte(compiler, OP_LOADTRUE); break;
        default: return; /* Unreachable */
    }
}

/**
 * Handles a map literal by creating a new Falcon map and compiling each of its elements.
 */
PARSE_RULE(map) {
    (void) canAssign; /* Unused */
    uint16_t entriesCount = 0;

    /* Parses the map entries */
    if (!check(compiler->parser, TK_RBRACE)) {
        do {
            expression(compiler); /* The key */

            if (entriesCount == UINT16_MAX) /* Reached max entries? */
                compiler_error(compiler, &compiler->parser->previous, COMP_MAP_LIMIT_ERR);

            consume(compiler, TK_COLON, COMP_MAP_COLON_ERR);
            expression(compiler); /* The value */
            entriesCount++;
        } while (match(compiler, TK_COMMA));
    }

    consume(compiler, TK_RBRACE, COMP_MAP_BRACE_ERR);
    emit_collection(compiler, OP_DEFMAP, entriesCount); /* Creates the new map */
}

/**
 * Handles a number literal by adding it to the constants table.
 */
PARSE_RULE(number) {
    (void) canAssign; /* Unused */
    emit_constant(compiler, compiler->parser->previous.value);
}

/**
 * Handles a string literal by adding it to the constants table.
 */
PARSE_RULE(string) {
    (void) canAssign; /* Unused */
    emit_constant(compiler, compiler->parser->previous.value);
}

/**
 * Handles the subscript (list indexing) operator expression by compiling the index expression.
 */
PARSE_RULE(subscript) {
    expression(compiler); /* Compiles the subscript index */
    consume(compiler, TK_RBRACKET, COMP_SUB_BRACKET_ERR);

    /* Compiles subscript assignments or access */
    if (canAssign && match(compiler, TK_EQUAL)) { /* a[i] = ... */
        expression(compiler);
        emit_byte(compiler, OP_SETSUB);
    } else { /* Access subscript */
        emit_byte(compiler, OP_GETSUB);
    }
}

/**
 * Handles a "super" access expression. Both the current receiver and the superclass are looked up,
 * and pushed onto the stack. If the expression is a direct "super" call, the more efficient
 * "OP_INVSUPER" instruction is emitted.
 */
PARSE_RULE(super_) {
    (void) canAssign; /* Unused */

    if (compiler->cCompiler == NULL) { /* Checks if not inside a class */
        compiler_error(compiler, &compiler->parser->previous, COMP_SUPER_ERR);
    } else if (!compiler->cCompiler->hasSuper) { /* Checks if there is no superclass */
        compiler_error(compiler, &compiler->parser->previous, COMP_NO_SUPER_ERR);
    }

    /* Compiles the identifier after "super." */
    consume(compiler, TK_DOT, COMP_SUPER_DOT_ERR);
    consume(compiler, TK_IDENTIFIER, COMP_SUPER_METHOD_ERR);
    uint8_t name = identifier_constant(compiler, &compiler->parser->previous);
    named_variable(compiler, synthetic_token("this"), false); /* Looks up the receiver */

    /* Compiles the "super" expression as a call or as an access */
    if (match(compiler, TK_LPAREN)) {
        uint8_t argCount = argument_list(compiler);
        named_variable(compiler, synthetic_token("super"), false);
        emit_bytes(compiler, OP_INVSUPER, name);
        emit_byte(compiler, argCount);
    } else {
        named_variable(compiler, synthetic_token("super"), false);
        emit_bytes(compiler, OP_SUPER, name); /* Performs the "super" access */
    }
}

/**
 * Handles the ternary "?:" conditional operator expression.
 */
PARSE_RULE(ternary) {
    (void) canAssign;                             /* Unused */
    int ifJump = emit_jump(compiler, OP_JUMPIFF); /* Jumps if the condition is false */
    emit_byte(compiler, OP_POPT);                 /* Pops the condition result */
    parse_precedence(compiler, PREC_TERNARY);     /* Compiles the first branch */
    consume(compiler, TK_COLON, COMP_TERNARY_EXPR_ERR);

    int elseJump = emit_jump(compiler, OP_JUMP); /* Jumps the second branch if first was taken */
    patch_jump(compiler, ifJump);                /* Patches the jump over the first branch */
    emit_byte(compiler, OP_POPT);                /* Pops the condition result */
    parse_precedence(compiler, PREC_ASSIGN);     /* Compiles the second branch */
    patch_jump(compiler, elseJump);              /* Patches the jump over the second branch */
}

/**
 * Handles a "this" expression, where "this" is treated as a lexically-scoped local variable.
 */
PARSE_RULE(this_) {
    (void) canAssign;                  /* Unused */
    if (compiler->cCompiler == NULL) { /* Checks if not inside a class */
        compiler_error(compiler, &compiler->parser->previous, COMP_THIS_ERR);
        return;
    }

    variable(compiler, false); /* "canAssign" is set to false */
}

/**
 * Handles a unary expression by compiling the operand and then emitting the bytecode to perform
 * the unary operation itself.
 */
PARSE_RULE(unary) {
    (void) canAssign; /* Unused */
    FalconTokens operatorType = compiler->parser->previous.type;
    parse_precedence(compiler, PREC_UNARY); /* Compiles the operand */

    switch (operatorType) {
        case TK_MINUS: emit_byte(compiler, OP_NEG); break;
        case TK_NOT: emit_byte(compiler, OP_NOT); break;
        default: return;
    }
}

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
    RULE(grouping, call, PREC_TOP),    /* TK_LPAREN */
    EMPTY_RULE,                        /* TK_RPAREN */
    PREFIX_RULE(map),                  /* TK_LBRACE */
    EMPTY_RULE,                        /* TK_RBRACE */
    RULE(list, subscript, PREC_TOP),   /* TK_LBRACKET */
    EMPTY_RULE,                        /* TK_RBRACKET */
    EMPTY_RULE,                        /* TK_COMMA */
    INFIX_RULE(dot, PREC_TOP),         /* TK_DOT */
    EMPTY_RULE,                        /* TK_COLON */
    EMPTY_RULE,                        /* TK_SEMICOLON */
    EMPTY_RULE,                        /* TK_ARROW */
    RULE(unary, binary, PREC_TERM),    /* TK_MINUS */
    INFIX_RULE(binary, PREC_TERM),     /* TK_PLUS */
    INFIX_RULE(binary, PREC_FACTOR),   /* TK_SLASH */
    INFIX_RULE(binary, PREC_FACTOR),   /* TK_PERCENT */
    INFIX_RULE(binary, PREC_FACTOR),   /* TK_STAR */
    INFIX_RULE(pow_, PREC_POW),        /* TK_CIRCUMFLEX */
    PREFIX_RULE(unary),                /* TK_NOT */
    INFIX_RULE(binary, PREC_EQUAL),    /* TK_NOTEQUAL */
    EMPTY_RULE,                        /* TK_EQUAL */
    INFIX_RULE(binary, PREC_EQUAL),    /* TK_EQEQUAL */
    INFIX_RULE(binary, PREC_COMPARE),  /* TK_GREATER */
    INFIX_RULE(binary, PREC_COMPARE),  /* TK_GREATEREQUAL */
    INFIX_RULE(binary, PREC_COMPARE),  /* TK_LESS */
    INFIX_RULE(binary, PREC_COMPARE),  /* TK_LESSEQUAL */
    INFIX_RULE(and_, PREC_AND),        /* TK_AND */
    INFIX_RULE(or_, PREC_OR),          /* TK_OR */
    INFIX_RULE(ternary, PREC_TERNARY), /* TK_QUESTION */
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
    PREFIX_RULE(super_),               /* TK_SUPER */
    EMPTY_RULE,                        /* TK_SWITCH */
    PREFIX_RULE(this_),                /* TK_THIS */
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
static void parse_precedence(FalconCompiler *compiler, PrecedenceLevels precedence) {
    advance(compiler);
    Parser *parser = compiler->parser;
    ParseFunction prefixRule = get_parse_rule(parser->previous.type)->prefix;

    if (prefixRule == NULL) { /* Checks if is a parsing error */
        compiler_error(compiler, &parser->previous, COMP_EXPR_ERR);
        return;
    }

    /* Checks if the left side is assignable */
    bool canAssign = precedence <= PREC_TERNARY;
    prefixRule(compiler, canAssign);

    /* Looks for an infix parser for the next token */
    while (precedence <= get_parse_rule(parser->current.type)->precedence) {
        advance(compiler);
        ParseFunction infixRule = get_parse_rule(parser->previous.type)->infix;
        infixRule(compiler, canAssign);
    }

    if (canAssign && match(compiler, TK_EQUAL)) { /* Checks if is an invalid assignment */
        compiler_error(compiler, &parser->previous, COMP_INV_ASSG_ERR);
    }
}

/**
 * Returns the rule at a given index.
 */
static ParseRule *get_parse_rule(FalconTokens type) { return &rules[type]; }

/**
 * Compiles an expression by parsing the lowest precedence level, which subsumes all of the higher
 * precedence expressions too.
 */
static void expression(FalconCompiler *compiler) { parse_precedence(compiler, PREC_ASSIGN); }

/**
 * Compiles a block of code by parsing declarations and statements until a closing brace (end of
 * block) is found.
 */
static void block(FalconCompiler *compiler) {
    while (!check(compiler->parser, TK_RBRACE) && !check(compiler->parser, TK_EOF)) {
        declaration(compiler);
    }

    consume(compiler, TK_RBRACE, COMP_BLOCK_BRACE_ERR);
}

/**
 * Compiles a function body and its parameters.
 */
static void function(FalconCompiler *compiler, FunctionType type) {
    Parser *parser = compiler->parser;
    FunctionCompiler fCompiler;
    init_function_compiler(compiler, &fCompiler, compiler->fCompiler, type);
    begin_scope(compiler->fCompiler);

    /* Compiles the parameter list */
    consume(compiler, TK_LPAREN, COMP_FUNC_NAME_PAREN_ERR);
    if (!check(parser, TK_RPAREN)) {
        do {
            ObjFunction *current = current_function(compiler->fCompiler);
            current->arity++;

            if (current->arity > UINT8_MAX) /* Exceeds the limit os parameters? */
                compiler_error(compiler, &parser->current, COMP_PARAMS_LIMIT_ERR);

            uint8_t paramConstant = parse_variable(compiler, COMP_PARAM_NAME_ERR);
            define_variable(compiler, paramConstant);
        } while (match(compiler, TK_COMMA));
    }
    consume(compiler, TK_RPAREN, COMP_FUNC_LIST_PAREN_ERR);

    /* Compiles the function body */
    consume(compiler, TK_LBRACE, COMP_FUNC_BODY_BRACE_ERR);
    block(compiler);

    /* Create the function object */
    ObjFunction *function = end_function_compiler(compiler);
    emit_bytes(compiler, OP_CLOSURE, make_constant(compiler, OBJ_VAL(function)));

    /* Emits the captured upvalues */
    for (int i = 0; i < function->upvalueCount; i++) {
        emit_byte(compiler, (uint8_t)(fCompiler.upvalues[i].isLocal ? true : false));
        emit_byte(compiler, fCompiler.upvalues[i].index);
    }
}

/**
 * Compiles a method definition inside a class declaration.
 */
static void method(FalconCompiler *compiler) {
    Parser *parser = compiler->parser;
    consume(compiler, TK_IDENTIFIER, COMP_METHOD_NAME_ERR);
    uint8_t nameConstant = identifier_constant(compiler, &parser->previous);

    /* Gets the method type */
    FunctionType type = TYPE_METHOD;
    if (parser->previous.length == 4 && memcmp(parser->previous.start, "init", 4) == 0) {
        type = TYPE_INIT;
    }

    /* Parses the method declaration as a function declaration */
    function(compiler, type);
    emit_bytes(compiler, OP_DEFMETHOD, nameConstant);
}

/**
 * Compiles a class declaration.
 */
static void class_declaration(FalconCompiler *compiler) {
    Parser *parser = compiler->parser;
    consume(compiler, TK_IDENTIFIER, COMP_CLASS_NAME_ERR);
    Token className = parser->previous;

    /* Adds the class name to the constants table and set the variable */
    uint8_t nameConstant = identifier_constant(compiler, &className);
    declare_variable(compiler);
    emit_bytes(compiler, OP_DEFCLASS, nameConstant);
    define_variable(compiler, nameConstant);

    /* Sets a new class compiler */
    ClassCompiler classCompiler;
    classCompiler.name = className;
    classCompiler.hasSuper = false;
    classCompiler.enclosing = compiler->cCompiler;
    compiler->cCompiler = &classCompiler;

    /* Parses a inheritance definition before the class body, so that the subclass can correctly
     * override inherited methods */
    if (match(compiler, TK_LESS)) {
        consume(compiler, TK_IDENTIFIER, COMP_SUPERCLASS_NAME_ERR);
        variable(compiler, false); /* Loads the superclass as a variable */

        if (identifiers_equal(&className, &parser->previous))
            compiler_error(compiler, &parser->previous, COMP_INHERIT_SELF_ERR);

        /* Creates a new scope and make the superclass a local variable */
        begin_scope(compiler->fCompiler);
        add_local(compiler, synthetic_token("super")); /* Creates a synthetic token for super */
        define_variable(compiler, 0);

        named_variable(compiler, className, false); /* Loads subclass as a variable too */
        emit_byte(compiler, OP_INHERIT);            /* Applies inheritance to the subclass */
        classCompiler.hasSuper = true;
    }

    /* Parses the class body and its methods */
    named_variable(compiler, className, false);
    consume(compiler, TK_LBRACE, COMP_CLASS_BODY_BRACE_ERR);
    while (!check(parser, TK_RBRACE) && !check(parser, TK_EOF)) method(compiler);

    consume(compiler, TK_RBRACE, COMP_CLASS_BODY_BRACE2_ERR);
    emit_byte(compiler, OP_POPT);
    if (classCompiler.hasSuper) end_scope(compiler);
    compiler->cCompiler = compiler->cCompiler->enclosing;
}

/**
 * Compiles a function declaration.
 */
static void fn_declaration(FalconCompiler *compiler) {
    uint8_t func = parse_variable(compiler, COMP_FUNC_NAME_ERR);
    mark_as_initialized(compiler->fCompiler);
    function(compiler, TYPE_FUNCTION);
    define_variable(compiler, func);
}

/**
 * Compiles the declaration of a single variable.
 */
static void single_var_declaration(FalconCompiler *compiler) {
    uint8_t global = parse_variable(compiler, COMP_VAR_NAME_ERR); /* Parses a variable name */

    if (match(compiler, TK_EQUAL)) {
        expression(compiler); /* Compiles the variable initializer */
    } else {
        emit_byte(compiler, OP_LOADNULL); /* Default variable value is "null" */
    }

    define_variable(compiler, global); /* Emits the declaration bytecode */
}

/**
 * Compiles a variable declaration list.
 */
static void var_declaration(FalconCompiler *compiler) {
    Parser *parser = compiler->parser;
    if (!check(parser, TK_SEMICOLON)) {
        do {
            single_var_declaration(compiler); /* Compiles each declaration */
        } while (match(compiler, TK_COMMA));
    }

    consume(compiler, TK_SEMICOLON, COMP_VAR_DECL_ERR);
}

/**
 * Compiles an expression statement.
 */
static void expression_statement(FalconCompiler *compiler) {
    expression(compiler);
    consume(compiler, TK_SEMICOLON, COMP_EXPR_STMT_ERR);
    bool retRepl = compiler->vm->isREPL && compiler->fCompiler->scopeDepth == COMP_GLOBAL_SCOPE;
    emit_byte(compiler, retRepl ? OP_POPEXPR : OP_POPT);
}

/**
 * Compiles an "if" conditional statement.
 */
static void if_statement(FalconCompiler *compiler) {
    expression(compiler); /* Compiles condition */
    consume(compiler, TK_LBRACE, COMP_IF_STMT_ERR);

    int thenJump = emit_jump(compiler, OP_JUMPIFF);
    emit_byte(compiler, OP_POPT);

    /* Compiles the "if" block */
    begin_scope(compiler->fCompiler);
    block(compiler);
    end_scope(compiler);

    int elseJump = emit_jump(compiler, OP_JUMP);
    patch_jump(compiler, thenJump);
    emit_byte(compiler, OP_POPT);

    /* Compiles the "else" block */
    if (match(compiler, TK_ELSE)) {
        if (match(compiler, TK_IF)) {
            if_statement(compiler); /* "else if ..." form */
        } else if (match(compiler, TK_LBRACE)) {
            begin_scope(compiler->fCompiler);
            block(compiler); /* Compiles the "else" branch */
            end_scope(compiler);
        }
    }

    patch_jump(compiler, elseJump);
}

/**
 * Compiles a "switch" conditional statement.
 */
static void switch_statement(FalconCompiler *compiler) {
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
    consume(compiler, TK_LBRACE, COMP_SWITCH_STMT_ERR);

    while (!match(compiler, TK_RBRACE) && !check(parser, TK_EOF)) {
        if (match(compiler, TK_WHEN) || match(compiler, TK_ELSE)) {
            FalconTokens caseType = parser->previous.type;

            if (switchState == FALCON_AFT_ELSE) { /* Already compiled the else case? */
                compiler_error(compiler, &parser->previous, COMP_ELSE_END_ERR);
            } else if (switchState == FALCON_BEF_ELSE) { /* Else case not compiled yet? */
                caseEnds[caseCount++] = emit_jump(compiler, OP_JUMP); /* Jumps the other cases */
                patch_jump(compiler, previousCaseSkip);               /* Patches the jump */
                emit_byte(compiler, OP_POPT);
            }

            if (caseType == TK_WHEN) {
                switchState = FALCON_BEF_ELSE;

                /* Checks if the case is equal to the switch value */
                emit_byte(compiler, OP_DUPT); /* "==" pops its operand, so duplicate before */
                expression(compiler);
                consume(compiler, TK_ARROW, COMP_ARR_CASE_ERR);
                emit_byte(compiler, OP_EQUAL);
                previousCaseSkip = emit_jump(compiler, OP_JUMPIFF);

                emit_byte(compiler, OP_POPT); /* Pops the comparison result */
            } else {
                switchState = FALCON_AFT_ELSE;
                consume(compiler, TK_ARROW, COMP_ARR_ELSE_ERR);
                previousCaseSkip = -1;
            }
        } else {
            if (switchState == FALCON_BEF_CASES) /* Statement outside a case? */
                compiler_error(compiler, &parser->previous, COMP_STMT_SWITCH_ERR);
            statement(compiler); /* Statement is inside a case */
        }
    }

    /* If no else case, patch its condition jump */
    if (switchState == FALCON_BEF_ELSE) {
        patch_jump(compiler, previousCaseSkip);
        emit_byte(compiler, OP_POPT);
    }

    /* Patch all the case jumps to the end */
    for (int i = 0; i < caseCount; i++) patch_jump(compiler, caseEnds[i]);

    emit_byte(compiler, OP_POPT); /* Pops the switch value */

#undef FALCON_BEF_CASES
#undef FALCON_BEF_ELSE
#undef FALCON_AFT_ELSE
}

/* Starts the compilation of a new loop by setting the entry point to the current bytecode chunk
 * instruction */
#define START_LOOP(fCompiler)                        \
    Loop loop;                                       \
    loop.enclosing = (fCompiler)->loop;              \
    loop.entry = current_bytecode(fCompiler)->count; \
    loop.scopeDepth = (fCompiler)->scopeDepth;       \
    (fCompiler)->loop = &loop

/* Compiles the body of a loop and sets its index */
#define LOOP_BODY(compiler)                                                            \
    compiler->fCompiler->loop->body = (compiler)->fCompiler->function->bytecode.count; \
    block(compiler)

/**
 * Gets the number of arguments for a instruction at the given program counter.
 * TODO: check if all opcodes are covered.
 */
int get_instruction_args(const BytecodeChunk *bytecode, int pc) {
    switch (bytecode->code[pc]) {
        case OP_LOADFALSE:
        case OP_LOADTRUE:
        case OP_LOADNULL:
        case OP_GETSUB:
        case OP_SETSUB:
        case OP_NOT:
        case OP_EQUAL:
        case OP_GREATER:
        case OP_LESS:
        case OP_ADD:
        case OP_SUB:
        case OP_NEG:
        case OP_DIV:
        case OP_MOD:
        case OP_MULT:
        case OP_POW:
        case OP_CLOSEUPVAL:
        case OP_RETURN:
        case OP_DUPT:
        case OP_POPT:
        case OP_POPEXPR:
        case OP_TEMP: return 0; /* Instructions with no arguments */

        case OP_DEFGLOBAL:
        case OP_GETGLOBAL:
        case OP_SETGLOBAL:
        case OP_GETUPVAL:
        case OP_SETUPVAL:
        case OP_GETLOCAL:
        case OP_SETLOCAL:
        case OP_CALL:
        case OP_DEFCLASS:
        case OP_DEFMETHOD:
        case OP_GETPROP:
        case OP_SETPROP:
        case OP_INVPROP: return 1; /* Instructions with single byte as argument */

        case OP_LOADCONST:
        case OP_DEFLIST:
        case OP_DEFMAP:
        case OP_AND:
        case OP_OR:
        case OP_JUMP:
        case OP_JUMPIFF:
        case OP_LOOP: return 2; /* Instructions with 2 bytes as arguments */

        case OP_CLOSURE: {
            int index = bytecode->code[pc + 1];
            ObjFunction *function = AS_FUNCTION(bytecode->constants.values[index]);
            return 1 + function->upvalueCount * 2; /* Function: 1 byte; Upvalues: 2 bytes each */
        }

        default: return 0;
    }
}

/**
 * Ends the current innermost loop on the compiler. If any temporary "OP_TEMP" instruction is in
 * the bytecode, replaces it with the correct "OP_JUMP" instruction and patches the jump to the
 * end of the loop.
 */
static void end_loop(FalconCompiler *compiler) {
    FunctionCompiler *fCompiler = compiler->fCompiler;
    BytecodeChunk *bytecode = &fCompiler->function->bytecode;
    int index = fCompiler->loop->body;

    while (index < bytecode->count) {
        if (bytecode->code[index] == OP_TEMP) { /* Is it a temporary for a "break"? */
            bytecode->code[index] = OP_JUMP;    /* Set the correct "OP_JUMP" instruction */
            patch_jump(compiler, index + 1);    /* Patch the jump to the end of the loop */
            index += 3;
        } else { /* Jumps the instruction and its arguments */
            index += 1 + get_instruction_args(bytecode, index); /* +1 byte - instruction */
        }
    }

    fCompiler->loop = fCompiler->loop->enclosing;
}

/**
 * Compiles a "while" loop statement.
 */
static void while_statement(FalconCompiler *compiler) {
    FunctionCompiler *fCompiler = compiler->fCompiler;

    START_LOOP(fCompiler); /* Starts a bew loop */
    expression(compiler);  /* Compiles the loop condition */
    consume(compiler, TK_LBRACE, COMP_WHILE_STMT_ERR);
    int exitJump = emit_jump(compiler, OP_JUMPIFF);
    emit_byte(compiler, OP_POPT);

    /* Compiles the "while" block */
    begin_scope(fCompiler);
    LOOP_BODY(compiler);
    end_scope(compiler);

    /* Emits the loop and patches the next jump */
    emit_loop(compiler, fCompiler->loop->entry);
    patch_jump(compiler, exitJump);
    emit_byte(compiler, OP_POPT);
    end_loop(compiler); /* Ends the loop */
}

/**
 * Compiles a "for" loop statement.
 */
static void for_statement(FalconCompiler *compiler) {
    FunctionCompiler *fCompiler = compiler->fCompiler;
    begin_scope(compiler->fCompiler); /* Begins the loop scope */

    /* Compiles the initializer clause */
    if (match(compiler, TK_COMMA)) { /* Empty initializer? */
        compiler_error(compiler, &compiler->parser->previous, COMP_FOR_STMT_INIT_ERR);
    } else {
        single_var_declaration(compiler); /* Variable declaration initializer */
        consume(compiler, TK_COMMA, COMP_FOR_STMT_CM1_ERR);
    }

    START_LOOP(fCompiler); /* Starts a bew loop */

    /* Compiles the conditional clause */
    expression(compiler);
    consume(compiler, TK_COMMA, COMP_FOR_STMT_CM2_ERR);
    int exitJump = emit_jump(compiler, OP_JUMPIFF);
    emit_byte(compiler, OP_POPT); /* Pops condition */

    /* Compiles the increment clause */
    int bodyJump = emit_jump(compiler, OP_JUMP);
    int incrementStart = current_bytecode(fCompiler)->count;
    expression(compiler);
    emit_byte(compiler, OP_POPT); /* Pops increment */
    consume(compiler, TK_LBRACE, COMP_FOR_STMT_BRC_ERR);
    emit_loop(compiler, fCompiler->loop->entry);
    fCompiler->loop->entry = incrementStart;
    patch_jump(compiler, bodyJump);

    LOOP_BODY(compiler); /* Compiles the "for" block */
    emit_loop(compiler, fCompiler->loop->entry);
    if (exitJump != -1) {
        patch_jump(compiler, exitJump);
        emit_byte(compiler, OP_POPT); /* Pops condition */
    }

    end_scope(compiler); /* Ends the loop scope */
    end_loop(compiler);  /* Ends the loop */
}

#undef START_LOOP
#undef LOOP_BODY

/* Checks if the current scope is outside of a loop body */
#define CHECK_LOOP_ERROR(fCompiler, error)                                \
    do {                                                                  \
        if ((fCompiler)->loop == NULL) /* Is outside of a loop body? */   \
            compiler_error(compiler, &compiler->parser->previous, error); \
    } while (false)

/**
 * Discards the local variables or upvalues created/captured in a loop scope.
 */
static void discard_locals_in_loop(FalconCompiler *compiler) {
    FunctionCompiler *fCompiler = compiler->fCompiler;
    for (int i = fCompiler->localCount - 1;
         i >= 0 && fCompiler->locals[i].depth > fCompiler->loop->scopeDepth; i--) {
        if (fCompiler->locals[fCompiler->localCount - 1].isCaptured) {
            emit_byte(compiler, OP_CLOSEUPVAL);
        } else {
            emit_byte(compiler, OP_POPT);
        }
    }
}

/**
 * Compiles a "break" control flow statement, breaking the current iteration of a loop and
 * discarding locals created.
 */
static void break_statement(FalconCompiler *compiler) {
    FunctionCompiler *fCompiler = compiler->fCompiler;
    CHECK_LOOP_ERROR(fCompiler, COMP_BREAK_LOOP_ERR); /* Checks if not inside a loop */
    consume(compiler, TK_SEMICOLON, COMP_BREAK_STMT_ERR);
    discard_locals_in_loop(compiler); /* Discards locals created in loop */

    /* Emits a temporary instruction to signal a "break" statement. It should become an "OP_JUMP"
     * instruction, once the size of the loop body is known */
    emit_jump(compiler, OP_TEMP);
}

/**
 * Compiles a "next" control flow statement, advancing to the next iteration of a loop and
 * discarding locals created.
 */
static void next_statement(FalconCompiler *compiler) {
    FunctionCompiler *fCompiler = compiler->fCompiler;
    CHECK_LOOP_ERROR(fCompiler, COMP_NEXT_LOOP_ERR); /* Checks if not inside a loop */
    consume(compiler, TK_SEMICOLON, COMP_NEXT_STMT_ERR);
    discard_locals_in_loop(compiler); /* Discards locals created in loop */

    /* Jumps to the entry point of the current innermost loop */
    emit_loop(compiler, fCompiler->loop->entry);
}

#undef CHECK_LOOP_ERROR

/**
 * Compiles a "return" statement.
 */
static void return_statement(FalconCompiler *compiler) {
    if (compiler->fCompiler->type == TYPE_SCRIPT) /* Checks if in top level code */
        compiler_error(compiler, &compiler->parser->previous, COMP_RETURN_TOP_LEVEL_ERR);

    if (match(compiler, TK_SEMICOLON)) {
        emit_return(compiler);
    } else {
        if (compiler->fCompiler->type == TYPE_INIT) /* Checks if inside a "init" method */
            compiler_error(compiler, &compiler->parser->previous, COMP_RETURN_INIT_ERR);

        /* Compiles the returned value */
        expression(compiler);
        consume(compiler, TK_SEMICOLON, COMP_RETURN_STMT_ERR);
        emit_byte(compiler, OP_RETURN);
    }
}

/**
 * Compiles a statement.
 */
static void statement(FalconCompiler *compiler) {
    if (match(compiler, TK_IF)) {
        if_statement(compiler);
    } else if (match(compiler, TK_SWITCH)) {
        switch_statement(compiler);
    } else if (match(compiler, TK_WHILE)) {
        while_statement(compiler);
    } else if (match(compiler, TK_FOR)) {
        for_statement(compiler);
    } else if (match(compiler, TK_BREAK)) {
        break_statement(compiler);
    } else if (match(compiler, TK_NEXT)) {
        next_statement(compiler);
    } else if (match(compiler, TK_RETURN)) {
        return_statement(compiler);
    } else {
        expression_statement(compiler);
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
            case TK_BREAK:
            case TK_CLASS:
            case TK_FOR:
            case TK_FUNCTION:
            case TK_IF:
            case TK_NEXT:
            case TK_RETURN:
            case TK_SWITCH:
            case TK_VAR:
            case TK_WHILE: return;
            default:; /* Keep skipping tokens */
        }

        advance(compiler);
    }
}

/**
 * Compiles a declaration statement.
 */
static void declaration(FalconCompiler *compiler) {
    if (match(compiler, TK_CLASS)) {
        class_declaration(compiler);
    } else if (match(compiler, TK_FUNCTION)) {
        fn_declaration(compiler);
    } else if (match(compiler, TK_VAR)) {
        var_declaration(compiler);
    } else {
        statement(compiler);
    }

    if (compiler->parser->panicMode) synchronize(compiler);
}

/**
 * Compiles a given source code string. The parsing technique used is a Pratt parser, an improved
 * recursive descent parser that associates semantics with tokens instead of grammar rules.
 */
ObjFunction *compile_source(FalconVM *vm, const char *source) {
    Parser parser;
    Scanner scanner;
    FalconCompiler programCompiler;
    FunctionCompiler fCompiler;

    /* Inits the parser, scanner, and compiler */
    init_parser(&parser);
    init_scanner(source, &scanner);
    init_compiler(&programCompiler, vm, &parser, &scanner);
    init_function_compiler(&programCompiler, &fCompiler, NULL, TYPE_SCRIPT);
    programCompiler.cCompiler = NULL;

    advance(&programCompiler);                 /* Get the first token */
    while (!match(&programCompiler, TK_EOF)) { /* Main compiler loop */
        declaration(&programCompiler);         /* Falcon's grammar entry point */
    }

    ObjFunction *function = end_function_compiler(&programCompiler);
    return programCompiler.parser->hadError ? NULL : function;
}
