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
    CONSTANT,  /* 2 bytes constant */
    FALSE_LIT, /* "false" literal */
    TRUE_LIT,  /* "true" literal */
    NULL_LIT,  /* "null" literal */

    /* Relational operations */
    AND,     /* 'and' logical operator */
    OR,      /* 'or' logical operator */
    NOT,     /* "!" operator */
    EQUAL,   /* "==" operator */
    GREATER, /* ">" operator */
    LESS,    /* "<" operator */

    /* Arithmetic operations */
    ADD,      /* "+" operator */
    SUBTRACT, /* "-" binary operator */
    NEGATE,   /* "-" unary operator */
    DIVIDE,   /* "/" operator */
    MOD,      /* "%" operator */
    MULTIPLY, /* "*" operator */
    POW,      /* "^" operator */

    /* Variable operations */
    DEFINE_GLOBAL, /* Define global variable */
    GET_GLOBAL,    /* Get global variable value */
    SET_GLOBAL,    /* Set global variable value */
    GET_UPVALUE,   /* Get upvalue value */
    SET_UPVALUE,   /* Set upvalue value */
    CLOSE_UPVALUE, /* Close upvalue */
    GET_LOCAL,     /* Get local variable value */
    SET_LOCAL,     /* Set local variable value */

    /* Jump/loop operations */
    JUMP,          /* Jump an instruction on the VM */
    JUMP_IF_FALSE, /* Jump an instruction if falsey on the VM */
    LOOP,          /* Loop backwards instruction */

    /* Closures/functions operations */
    CLOSURE, /* Closure declaration */
    CALL,    /* Perform a function call */
    RETURN,  /* "return" statement */

    /* VM operations */
    DUP,      /* Duplicate the top of the VM stack */
    POP,      /* Pop from the VM stack */
    POP_EXPR, /* Pop and print expression value */
    TEMP      /* Mark a temporary value */

} FalconOpCodes;

#endif // FALCON_OPCODES_H
