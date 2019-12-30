/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * falcon_compiler.h: Falcons's handwritten Parser/Compiler based on Pratt Parsing
 * See Falcon's license in the LICENSE file
 */

#ifndef FALCON_COMPILER_H
#define FALCON_COMPILER_H

#include "../falcon.h"
#include "../vm/falcon_object.h"
#include "falcon_scanner.h"

/* Function types:
 * - TYPE_FUNCTION represents an user-defined function
 * - TYPE_SCRIPT represents the top-level (global scope) code */
typedef enum { TYPE_FUNCTION, TYPE_SCRIPT } FunctionType;

/* Local variable representation */
typedef struct {
    Token name;      /* The identifier of the local variable */
    int depth;       /* The depth in the scope chain where the local was declared */
    bool isCaptured; /* Whether the variable was captured as an upvalue */
} Local;

/* Upvalue representation */
typedef struct {
    uint8_t index; /* The index of the local/upvalue being captured */
    bool isLocal;  /* Whether the captured upvalue is a local variable in the enclosing function */
} Upvalue;

/* Loop representation */
typedef struct sLoop {
    struct sLoop *enclosing; /* The enclosing loop */
    int entry;               /* The index of the first loop instruction */
    int body;                /* The index of the first instruction of the loop's body */
    int scopeDepth;          /* Depth of the loop scope */
} Loop;

/* Function compiler representation */
typedef struct sCompiler {

    /* The compiler for the enclosing function or NULL (when the compiling code is at the top-level
     * (i.e., global scope) */
    struct sCompiler *enclosing;

    /* The function being compiled */
    ObjFunction *function;

    /* Whether the current scope is global (TYPE_SCRIPT) or local (TYPE_FUNCTION), and the current
     * depth of block scope nesting */
    FunctionType type;
    int scopeDepth;

    /* List of locals declared in the compiling function */
    Local locals[FALCON_MAX_BYTE];
    int localCount;

    /* List of upvalues captured from outer scope by the compiling function */
    Upvalue upvalues[FALCON_MAX_BYTE];

    /* The innermost loop being compiled or NULL if not in a loop */
    Loop *loop;

} FunctionCompiler;

/* Parser representation */
typedef struct {
    Token current;  /* The last "lexed" token */
    Token previous; /* The last consumed token */
    bool hadError;  /* Whether a syntax/compile error occurred or not */
    bool panicMode; /* Whether the parser is in error recovery (Panic Mode) or not */
} Parser;

/* Program compiler representation */
typedef struct {
    FalconVM *vm;                /* Falcon's virtual machine instance */
    Parser *parser;              /* Falcon's parser instance */
    Scanner *scanner;            /* Falcon's scanner instance */
    FunctionCompiler *fCompiler; /* The compiler for the currently compiling function */
} FalconCompiler;

/* Compiler operations */
ObjFunction *falconCompile(FalconVM *vm, const char *source);

/* Compilation flags */
#define FALCON_ERROR_STATE      (-1)
#define FALCON_UNDEFINED_SCOPE  FALCON_ERROR_STATE
#define FALCON_UNRESOLVED_LOCAL FALCON_ERROR_STATE
#define FALCON_GLOBAL_SCOPE     0

/* Compilation error messages */
/* Expressions */
#define FALCON_GRP_EXPR_ERR     "Expected a ')' after expression."
#define FALCON_TERNARY_EXPR_ERR "Expected a ':' after first branch of ternary operator."
#define FALCON_EXPR_ERR         "Expected an expression."
#define FALCON_EXPR_STMT_ERR    "Expected a ';' after expression."
#define FALCON_LIST_BRACKET_ERR "Expected a ']' after list elements."
#define FALCON_SUB_BRACKET_ERR  "Expected a ']' after subscript expression."

/* Conditionals and Loops */
#define FALCON_IF_STMT_ERR       "Expected a '{' after 'if' condition."
#define FALCON_SWITCH_STMT_ERR   "Expected a '{' before switch cases."
#define FALCON_ELSE_END_ERR      "Cases or else are not allowed after an else case."
#define FALCON_ARR_CASE_ERR      "Expected a '->' after case."
#define FALCON_ARR_ELSE_ERR      "Expected a '->' after else case."
#define FALCON_STMT_SWITCH_ERR   "Cannot have statements before any switch case."
#define FALCON_WHILE_STMT_ERR    "Expected a '{' after 'while' condition."
#define FALCON_FOR_STMT_INIT_ERR "Expected an implicit variable declaration in the init clause."
#define FALCON_FOR_STMT_CM1_ERR  "Expected a ',' after 'for' loop init clause."
#define FALCON_FOR_STMT_CM2_ERR  "Expected a ',' after 'for' loop conditional clause."
#define FALCON_FOR_STMT_BRC_ERR  "Expected a '{' after 'for' loop increment clause."
#define FALCON_NEXT_STMT_ERR     "Expected a ';' after 'next' statement."
#define FALCON_NEXT_LOOP_ERR     "'next' statement outside of a loop body."
#define FALCON_BREAK_STMT_ERR    "Expected a ';' after 'break' statement."
#define FALCON_BREAK_LOOP_ERR    "'break' statement outside of a loop body."

/* Variables */
#define FALCON_RED_INIT_ERR   "Cannot read variable in its own initializer."
#define FALCON_VAR_REDECL_ERR "Variable or closure already declared in this scope."
#define FALCON_INV_ASSG_ERR   "Invalid assignment target."
#define FALCON_VAR_NAME_ERR   "Expected a variable name."
#define FALCON_VAR_DECL_ERR   "Expected a ';' after variable declaration."

/* Functions and Blocks */
#define FALCON_BLOCK_BRACE_ERR      "Expected a '}' after block."
#define FALCON_CALL_LIST_PAREN_ERR  "Expected a ')' after function arguments."
#define FALCON_FUNC_NAME_PAREN_ERR  "Expected a '(' after function name."
#define FALCON_FUNC_LIST_PAREN_ERR  "Expected a ')' after function parameters."
#define FALCON_FUNC_BODY_BRACE_ERR  "Expected a '{' before function body."
#define FALCON_FUNC_NAME_ERR        "Expected a function name."
#define FALCON_PARAM_NAME_ERR       "Expected a parameter name."
#define FALCON_RETURN_STMT_ERR      "Expected a ';' after return value."
#define FALCON_RETURN_TOP_LEVEL_ERR "Cannot return from top level code."

/* Limits */
#define FALCON_CONST_LIMIT_ERR   "Limit of 65535 constants reached."
#define FALCON_LOOP_LIMIT_ERR    "Limit of 65535 instructions in loop body reached."
#define FALCON_JUMP_LIMIT_ERR    "Limit of 65535 instructions in conditional branch reached."
#define FALCON_VAR_LIMIT_ERR     "Limit of 255 local variables in scope reached."
#define FALCON_CLOSURE_LIMIT_ERR "Limit of 255 closure variables reached."
#define FALCON_ARGS_LIMIT_ERR    "Limit of 255 arguments reached."
#define FALCON_PARAMS_LIMIT_ERR  "Limit of 255 parameters reached."

#endif // FALCON_COMPILER_H
