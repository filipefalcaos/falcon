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
    OP_LOADCONST, /* 2 bytes constant */
    OP_LOADFALSE, /* "false" literal */
    OP_LOADTRUE,  /* "true" literal */
    OP_LOADNULL,  /* "null" literal */

    /* Lists and subscripts */
    OP_DEFLIST,  /* Define a new list */
    OP_PUSHLIST, /* Push a value to the end of a list */
    OP_GETSUB,   /* Get a list/string element by index */
    OP_SETSUB,   /* Set a list/string element by index */

    /* Relational operations */
    OP_AND,     /* "and" logical operator */
    OP_OR,      /* "or" logical operator */
    OP_NOT,     /* "not" operator */
    OP_EQUAL,   /* "==" operator */
    OP_GREATER, /* ">" operator */
    OP_LESS,    /* "<" operator */

    /* Arithmetic operations */
    OP_ADD,  /* "+" operator */
    OP_SUB,  /* "-" binary operator */
    OP_NEG,  /* "-" unary operator */
    OP_DIV,  /* "/" operator */
    OP_MOD,  /* "%" operator */
    OP_MULT, /* "*" operator */
    OP_POW,  /* "^" operator */

    /* Variable operations */
    OP_DEFGLOBAL,  /* Define a global variable */
    OP_GETGLOBAL,  /* Get the value of a global variable */
    OP_SETGLOBAL,  /* Set the value of a global variable */
    OP_GETUPVAL,   /* Get the value of a upvalue */
    OP_SETUPVAL,   /* Set the value of a upvalue */
    OP_CLOSEUPVAL, /* Close a upvalue */
    OP_GETLOCAL,   /* Get the value of a local variable */
    OP_SETLOCAL,   /* Set the value of a local variable */

    /* Jump/loop operations */
    OP_JUMP,        /* Jump an instruction on the VM */
    OP_JUMPIFFALSE, /* Jump an instruction if falsy on the VM */
    OP_LOOP,        /* Loop backwards to a instruction on the VM */

    /* Closures/functions operations */
    OP_CLOSURE, /* Closure declaration */
    OP_CALL,    /* Perform a function call */
    OP_RETURN,  /* "return" statement */

    /* Class operations */
    OP_DEFCLASS,  /* Define a new class */
    OP_DEFMETHOD, /* Define a new method in a class */
    OP_GETPROP,   /* Get the value of a property */
    OP_SETPROP,   /* Set the value of a property */
    OP_INVPROP,   /* Perform a invocation on a property */

    /* VM operations */
    OP_DUPTOP,     /* Duplicate the top of the VM stack */
    OP_POPTOP,     /* Pop from the VM stack */
    OP_POPTOPEXPR, /* Pop from the VM stack and print the top value */
    OP_TEMP        /* Mark a temporary value */

} FalconOpCodes;

#endif // FALCON_OPCODES_H
