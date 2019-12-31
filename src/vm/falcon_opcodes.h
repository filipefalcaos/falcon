/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * falcon_opcodes.h: Falcon's opcodes for the virtual machine
 * See Falcon's license in the LICENSE file
 */

#ifndef FALCON_OPCODES_H
#define FALCON_OPCODES_H

/* Falcon OpCodes */
typedef enum {

    /* Constants and literals */
    LOAD_CONST, /* 2 bytes constant */
    LOAD_FALSE, /* "false" literal */
    LOAD_TRUE,  /* "true" literal */
    LOAD_NULL,  /* "null" literal */

    /* Lists */
    DEF_LIST,      /* Create a new list */
    PUSH_LIST,     /* Push a value to the end of a list */
    GET_SUBSCRIPT, /* Get a list element by index */
    SET_SUBSCRIPT, /* Set a list element by index */

    /* Relational operations */
    BIN_AND,     /* 'and' logical operator */
    BIN_OR,      /* 'or' logical operator */
    UN_NOT,      /* "!" operator */
    BIN_EQUAL,   /* "==" operator */
    BIN_GREATER, /* ">" operator */
    BIN_LESS,    /* "<" operator */

    /* Arithmetic operations */
    BIN_ADD,  /* "+" operator */
    BIN_SUB,  /* "-" binary operator */
    UN_NEG,   /* "-" unary operator */
    BIN_DIV,  /* "/" operator */
    BIN_MOD,  /* "%" operator */
    BIN_MULT, /* "*" operator */
    BIN_POW,  /* "^" operator */

    /* Variable operations */
    DEF_GLOBAL,  /* Define global variable */
    GET_GLOBAL,  /* Get global variable value */
    SET_GLOBAL,  /* Set global variable value */
    GET_UPVALUE, /* Get upvalue value */
    SET_UPVALUE, /* Set upvalue value */
    CLS_UPVALUE, /* Close upvalue */
    GET_LOCAL,   /* Get local variable value */
    SET_LOCAL,   /* Set local variable value */

    /* Jump/loop operations */
    JUMP_FWR,      /* Jump an instruction on the VM */
    JUMP_IF_FALSE, /* Jump an instruction if falsy on the VM */
    LOOP_BACK,     /* Loop backwards instruction */

    /* Closures/functions operations */
    FN_CLOSURE, /* Closure declaration */
    FN_CALL,    /* Perform a function call */
    FN_RETURN,  /* "return" statement */

    /* VM operations */
    DUP_TOP,      /* Duplicate the top of the VM stack */
    POP_TOP,      /* Pop from the VM stack */
    POP_TOP_EXPR, /* Pop and print expression value */
    TEMP_MARK     /* Mark a temporary value */

} FalconOpCodes;

#endif // FALCON_OPCODES_H
