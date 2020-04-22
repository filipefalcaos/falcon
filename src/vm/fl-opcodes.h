/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * fl-opcodes.h: Falcon's opcodes for the virtual machine
 * See Falcon's license in the LICENSE file
 */

#ifndef FALCON_OPCODES_H
#define FALCON_OPCODES_H

/* clang-format off */

/* Falcon OpCodes */
typedef enum {

    /* Notes:
     * (i) the default size for an argument is one byte;
     * (ii) "Ax", "Bx", etc, means a two bytes argument;
     * (iii) "Pn" means the n-th pop operation on the instruction;
     * (iv) "St" means "stack top";
     * (v) "Jmp(A)" means jump A bytes forward; "Jmp(-A)" means jump A bytes backwards */

    /* ======================================================================================= */
    /* OpCode         | Arguments | Effect                                                     */
    /* ======================================================================================= */

    OP_LOADCONST,  /* | Ax        | Pushes a constant at index Ax to the stack                 */
    OP_LOADFALSE,  /* |           | Pushes the "false" boolean to the stack                    */
    OP_LOADTRUE,   /* |           | Pushes the "true" boolean to the stack                     */
    OP_LOADNULL,   /* |           | Pushes the "null" literal to the stack                     */

    OP_DEFLIST,    /* | Ax        | Pushes a new list object with Ax elements; pops Ax times   */
    OP_DEFMAP,     /* | Ax        | Pushes a new map object with Ax entries; pops Ax * 2 times */
    OP_GETSUB,     /* |           | Pushes the P2[P1] value to the stack                       */
    OP_SETSUB,     /* |           | Assigns P1 to P3[P2] and pushes P1 to the stack            */

    OP_AND,        /* | Ax        | If St is false, Jmp(Ax); else, pop                         */
    OP_OR,         /* | Ax        | If St is false, pop; else, Jmp(Ax)                         */
    OP_NOT,        /* |           | Replaces St with not St                                    */
    OP_EQUAL,      /* |           | Replaces St with St == P1                                  */
    OP_GREATER,    /* |           | Replaces St with St > P1                                   */
    OP_LESS,       /* |           | Replaces St with St < P1                                   */

    OP_ADD,        /* |           | Replaces St with P1 + St                                   */
    OP_SUB,        /* |           | Replaces St with P1 - St                                   */
    OP_NEG,        /* |           | Replaces St with -St                                       */
    OP_DIV,        /* |           | Replaces St with St / P1                                   */
    OP_MOD,        /* |           | Replaces St with St % P1                                   */
    OP_MULT,       /* |           | Replaces St with St * P1                                   */
    OP_POW,        /* |           | Replaces St with pow(St, P1)                               */

    OP_DEFGLOBAL,  /* | A         | Defines a global with name A and value P1                  */
    OP_GETGLOBAL,  /* | A         | Pushes the value of a global named A to the stack          */
    OP_SETGLOBAL,  /* | A         | Assigns St to a global named A                             */
    OP_GETUPVAL,   /* | A         | Pushes the upvalue at index A to the stack                 */
    OP_SETUPVAL,   /* | A         | Assigns St to the upvalue at index A                       */
    OP_CLOSEUPVAL, /* |           | Closes the upvalue for St and then pops it                 */
    OP_GETLOCAL,   /* | A         | Pushes the value of a local at slot A to the stack         */
    OP_SETLOCAL,   /* | A         | Assigns St to a local at slot A                            */

    OP_JUMP,       /* | Ax        | Jmp(Ax)                                                    */
    OP_JUMPIFF,    /* | Ax        | If St is false, Jmp(Ax)                                    */
    OP_LOOP,       /* | Ax        | Jmp(-Ax)                                                   */

    OP_CLOSURE,    /* | A         | Pushes a new closure object for the function at index A    */
    OP_CALL,       /* | A         | Pushes the return of a call (A args) to the stack          */
    OP_RETURN,     /* |           | Exits the current function and returns St                  */

    OP_DEFCLASS,   /* | A         | Pushes a new class object named A to the stack             */
    OP_INHERIT,    /* |           | Copies the methods from a superclass and pops the subclass */
    OP_DEFMETHOD,  /* | A         | Defines a new method object named A and then pops it       */
    OP_INVPROP,    /* | A B       | Invokes a method named A (B args) and pushes the result    */
    OP_GETPROP,    /* | A         | Pushes the prop named A from the instance P1               */
    OP_SETPROP,    /* | A         | Assigns St to the prop named A from the instance P1        */
    OP_SUPER,      /* | A         | Replaces St with the result of the "super" access from P1  */
    OP_INVSUPER,   /* | A B       | Invokes a method named A (B args) from the superclass P1   */

    OP_DUPT,       /* |           | Pushes St to the stack                                     */
    OP_POPT,       /* |           | Pops from the stack                                        */
    OP_POPEXPR,    /* |           | Pops from the stack and prints the old St                  */
    OP_TEMP        /* |           | Marks a compiler temporary (should not be executed)        */

} FalconOpCodes;

/* clang-format on */

#endif // FALCON_OPCODES_H
