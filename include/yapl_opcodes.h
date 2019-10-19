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

    /* Arithmetic operations */
    OP_ADD,      /* "+" operator */
    OP_SUBTRACT, /* "-" binary operator */
    OP_NEGATE,   /* "-" unary operator */
    OP_DIVIDE,   /* "/" operator */
    OP_MOD,      /* "%" operator */
    OP_MULTIPLY, /* "*" operator */

    /* Function operations */
    OP_RETURN /* "return" statement */

} OpCodes;

#endif // YAPL_OPCODES_H
