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
    OP_CONSTANT,  /* 2 bytes constant */
    OP_FALSE_LIT, /* "false" literal */
    OP_TRUE_LIT,  /* "true" literal */
    OP_NULL_LIT,  /* "null" literal */

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
    OP_POW,      /* "^" operator */

    /* Variable operations */
    OP_DEFINE_GLOBAL, /* Define global variable */
    OP_GET_GLOBAL,    /* Get global variable value */
    OP_SET_GLOBAL,    /* Set global variable value */
    OP_GET_UPVALUE,   /* Get upvalue value */
    OP_SET_UPVALUE,   /* Set upvalue value */
    OP_CLOSE_UPVALUE, /* Close upvalue */
    OP_GET_LOCAL,     /* Get local variable value */
    OP_SET_LOCAL,     /* Set local variable value */

    /* Jump/loop operations */
    OP_JUMP,          /* Jump an instruction on the VM */
    OP_JUMP_IF_FALSE, /* Jump an instruction if falsy on the VM */
    OP_LOOP,          /* Loop backwards instruction */

    /* Closures/functions operations */
    OP_CLOSURE, /* Closure declaration */
    OP_CALL,    /* Perform a function call */
    OP_RETURN,  /* "return" statement */

    /* VM operations */
    OP_DUP,      /* Duplicate the top of the VM stack */
    OP_POP,      /* Pop from the VM stack */
    OP_POP_EXPR, /* Pop and print expression value */
    OP_TEMP      /* Mark a temporary value */

} FalconOpCodes;

#endif // FALCON_OPCODES_H
