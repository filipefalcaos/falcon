/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * fl-compiler.h: Falcons's handwritten Parser/Compiler based on Pratt Parsing
 * See Falcon's license in the LICENSE file
 */

#ifndef FALCON_COMPILER_H
#define FALCON_COMPILER_H

#include "../falcon.h"
#include "../vm/fl-object.h"
#include "fl-scanner.h"

/* Function types: (i) TYPE_FUNCTION represents an user-defined function; (ii) TYPE_SCRIPT
 * represents the top-level (global scope) code; (iii) TYPE_METHOD represents a user-defined method
 * in a class; and (iv) TYPE_INIT represents the "init" method of a class */
typedef enum { TYPE_FUNCTION, TYPE_SCRIPT, TYPE_METHOD, TYPE_INIT } FunctionType;

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

/* Class compiler representation */
typedef struct cCompiler {

    /* The compiler for the enclosing class or NULL (when the compiling code is not inside a class
     * definition */
    struct cCompiler *enclosing;

    /* The name of the class being compiled */
    Token name;

    /* Whether the class being compiled has a superclass */
    bool hasSuper;

} ClassCompiler;

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
    ClassCompiler *cCompiler;    /* The compiler for the currently compiling class */
} FalconCompiler;

/* Compiler operations */
ObjFunction *falconCompile(FalconVM *vm, const char *source);

/* Compilation flags */
#define COMP_ERROR_STATE      (-1)
#define COMP_UNDEF_SCOPE      COMP_ERROR_STATE
#define COMP_UNRESOLVED_LOCAL COMP_ERROR_STATE
#define COMP_GLOBAL_SCOPE     0

/* Compilation error messages */
/* Expressions */
#define COMP_GRP_EXPR_ERR     "Expected a ')' after expression."
#define COMP_TERNARY_EXPR_ERR "Expected a ':' after first branch of ternary operator."
#define COMP_EXPR_ERR         "Expected an expression."
#define COMP_EXPR_STMT_ERR    "Expected a ';' after expression."
#define COMP_LIST_BRACKET_ERR "Expected a ']' after list elements."
#define COMP_SUB_BRACKET_ERR  "Expected a ']' after subscript expression."
#define COMP_MAP_COLON_ERR    "Expected a ':' after a map key."
#define COMP_MAP_BRACE_ERR    "Expected a '}' after map entries."

/* Conditionals and Loops */
#define COMP_IF_STMT_ERR       "Expected a '{' after 'if' condition."
#define COMP_SWITCH_STMT_ERR   "Expected a '{' before switch cases."
#define COMP_ELSE_END_ERR      "Cases or else are not allowed after an else case."
#define COMP_ARR_CASE_ERR      "Expected a '->' after case."
#define COMP_ARR_ELSE_ERR      "Expected a '->' after else case."
#define COMP_STMT_SWITCH_ERR   "Cannot have statements before any switch case."
#define COMP_WHILE_STMT_ERR    "Expected a '{' after 'while' condition."
#define COMP_FOR_STMT_INIT_ERR "Expected an implicit variable declaration in the init clause."
#define COMP_FOR_STMT_CM1_ERR  "Expected a ',' after 'for' loop init clause."
#define COMP_FOR_STMT_CM2_ERR  "Expected a ',' after 'for' loop conditional clause."
#define COMP_FOR_STMT_BRC_ERR  "Expected a '{' after 'for' loop increment clause."
#define COMP_NEXT_STMT_ERR     "Expected a ';' after 'next' statement."
#define COMP_NEXT_LOOP_ERR     "'next' statement outside of a loop body."
#define COMP_BREAK_STMT_ERR    "Expected a ';' after 'break' statement."
#define COMP_BREAK_LOOP_ERR    "'break' statement outside of a loop body."

/* Variables */
#define COMP_READ_INIT_ERR  "Cannot read variable in its own initializer."
#define COMP_VAR_REDECL_ERR "Variable or closure already declared in this scope."
#define COMP_INV_ASSG_ERR   "Invalid assignment target."
#define COMP_VAR_NAME_ERR   "Expected a variable name."
#define COMP_VAR_DECL_ERR   "Expected a ';' after variable declaration."

/* Functions and Blocks */
#define COMP_BLOCK_BRACE_ERR      "Expected a '}' after block."
#define COMP_CALL_LIST_PAREN_ERR  "Expected a ')' after function arguments."
#define COMP_FUNC_NAME_PAREN_ERR  "Expected a '(' after function name."
#define COMP_FUNC_LIST_PAREN_ERR  "Expected a ')' after function parameters."
#define COMP_FUNC_BODY_BRACE_ERR  "Expected a '{' before function body."
#define COMP_FUNC_NAME_ERR        "Expected a function name."
#define COMP_PARAM_NAME_ERR       "Expected a parameter name."
#define COMP_RETURN_STMT_ERR      "Expected a ';' after return value."
#define COMP_RETURN_TOP_LEVEL_ERR "Cannot return from top level code."

/* Classes */
#define COMP_CLASS_NAME_ERR        "Expected a class name."
#define COMP_SUPERCLASS_NAME_ERR   "Expected a superclass name."
#define COMP_METHOD_NAME_ERR       "Expected a method name."
#define COMP_CLASS_BODY_BRACE_ERR  "Expected a '{' before class body."
#define COMP_CLASS_BODY_BRACE2_ERR "Expected a '}' after class body."
#define COMP_PROP_NAME_ERR         "Expected a property name after a '.'."
#define COMP_THIS_ERR              "Cannot use 'this' outside of a class."
#define COMP_RETURN_INIT_ERR       "Cannot return from a 'init' method."
#define COMP_INHERIT_SELF_ERR      "A class cannot inherit from itself."
#define COMP_SUPER_ERR             "Cannot use 'super' outside of a class."
#define COMP_NO_SUPER_ERR          "Cannot use 'super' in a class that has no superclass."
#define COMP_SUPER_DOT_ERR         "Expected a '.' after 'super'."
#define COMP_SUPER_METHOD_ERR      "Expected a superclass method after 'super'."

/* Limits */
#define COMP_CONST_LIMIT_ERR   "Limit of 65535 constants reached."
#define COMP_LOOP_LIMIT_ERR    "Limit of 65535 instructions in loop body reached."
#define COMP_JUMP_LIMIT_ERR    "Limit of 65535 instructions in conditional branch reached."
#define COMP_LIST_LIMIT_ERR    "Limit of 65535 elements in a list reached."
#define COMP_MAP_LIMIT_ERR     "Limit of 65535 elements in a map reached."
#define COMP_VAR_LIMIT_ERR     "Limit of 255 local variables in scope reached."
#define COMP_CLOSURE_LIMIT_ERR "Limit of 255 closure variables reached."
#define COMP_ARGS_LIMIT_ERR    "Limit of 255 arguments reached."
#define COMP_PARAMS_LIMIT_ERR  "Limit of 255 parameters reached."

#endif // FALCON_COMPILER_H
