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
    FALCON_OP_CONSTANT, /* 2 bytes constant */
    FALCON_OP_FALSE,    /* "false" literal */
    FALCON_OP_TRUE,     /* "true" literal */
    FALCON_OP_NULL,     /* "null" literal */

    /* Relational operations */
    FALCON_OP_AND,     /* 'and' logical operator */
    FALCON_OP_OR,      /* 'or' logical operator */
    FALCON_OP_NOT,     /* "!" operator */
    FALCON_OP_EQUAL,   /* "==" operator */
    FALCON_OP_GREATER, /* ">" operator */
    FALCON_OP_LESS,    /* "<" operator */

    /* Arithmetic operations */
    FALCON_OP_ADD,      /* "+" operator */
    FALCON_OP_SUBTRACT, /* "-" binary operator */
    FALCON_OP_NEGATE,   /* "-" unary operator */
    FALCON_OP_DIVIDE,   /* "/" operator */
    FALCON_OP_MOD,      /* "%" operator */
    FALCON_OP_MULTIPLY, /* "*" operator */
    FALCON_OP_POW,      /* "^" operator */

    /* Variable operations */
    FALCON_OP_DEFINE_GLOBAL, /* Define global variable */
    FALCON_OP_GET_GLOBAL,    /* Get global variable value */
    FALCON_OP_SET_GLOBAL,    /* Set global variable value */
    FALCON_OP_GET_UPVALUE,   /* Get upvalue value */
    FALCON_OP_SET_UPVALUE,   /* Set upvalue value */
    FALCON_OP_CLOSE_UPVALUE, /* Close upvalue */
    FALCON_OP_GET_LOCAL,     /* Get local variable value */
    FALCON_OP_SET_LOCAL,     /* Set local variable value */

    /* Jump/loop operations */
    FALCON_OP_JUMP,          /* Jump an instruction on the VM */
    FALCON_OP_JUMP_IF_FALSE, /* Jump an instruction if falsey on the VM */
    FALCON_OP_LOOP,          /* Loop backwards instruction */

    /* Closures/functions operations */
    FALCON_OP_CLOSURE, /* Closure declaration */
    FALCON_OP_CALL,    /* Perform a function call */
    FALCON_OP_RETURN,  /* "return" statement */

    /* VM operations */
    FALCON_OP_DUP,      /* Duplicate the top of the VM stack */
    FALCON_OP_POP,      /* Pop from the VM stack */
    FALCON_OP_POP_EXPR, /* Pop and print expression value */
    FALCON_OP_TEMP      /* Mark a temporary value */

} FalconOpCodes;

#endif // FALCON_OPCODES_H
