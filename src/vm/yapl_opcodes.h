/*
 * YAPL version 0.0.1 (master, Oct 18 2019)
 * yapl_opcodes.h: YAPL's opcodes for the virtual machine
 * See YAPL's license in the LICENSE file
 */

#ifndef YAPL_OPCODES_H
#define YAPL_OPCODES_H

/* YAPL OpCodes */
typedef enum {

    /* Constants and literals */
    OP_CONSTANT,    /* 1 byte constant */
    OP_CONSTANT_16, /* 2 bytes constant */
    OP_FALSE,       /* "false" literal */
    OP_TRUE,        /* "true" literal */
    OP_NULL,        /* "null" literal */

    /* Relational operations */
    OP_AND,     /* 'and' logical operator */
    OP_OR,      /* 'or' logical operator */
    OP_NOT,     /* "!" operator */
    OP_EQUAL,   /* "==" operator */
    OP_GREATER, /* ">" operator */
    OP_LESS,    /* "<" operator */

    /* Arithmetic operations */
    OP_ADD,      /* "+" operator */
    OP_SUBTRACT, /* "-" binary operator */
    OP_NEGATE,   /* "-" unary operator */
    OP_DIVIDE,   /* "/" operator */
    OP_MOD,      /* "%" operator */
    OP_MULTIPLY, /* "*" operator */

    /* Variable operations */
    OP_DECREMENT,     /* "--" operator */
    OP_INCREMENT,     /* "++" operator */
    OP_DEFINE_GLOBAL, /* Define global variable */
    OP_GET_GLOBAL,    /* Get global variable value */
    OP_SET_GLOBAL,    /* Set global variable value */
    OP_GET_LOCAL,     /* Get local variable value */
    OP_SET_LOCAL,     /* Set local variable value */

    /* Jump/loop operations */
    OP_JUMP,          /* Jump an instruction on the VM */
    OP_JUMP_IF_FALSE, /* Jump an instruction if falsey on the VM */
    OP_LOOP,          /* Loop backwards instruction */

    /* Function operations */
    OP_CALL,   /* Perform a function call */
    OP_RETURN, /* "return" statement */

    /* VM operations */
    OP_POP,  /* Pop from VM stack */
    OP_POP_N /* Pop from VM stack "n" times */

} OpCodes;

#endif // YAPL_OPCODES_H
