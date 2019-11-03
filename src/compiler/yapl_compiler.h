/*
 * YAPL version 0.0.1 (master, Oct 18 2019)
 * yapl_compiler.h: YAPL's handwritten parser/compiler based on Pratt Parsing
 * See YAPL's license in the LICENSE file
 */

#ifndef YAPL_COMPILER_H
#define YAPL_COMPILER_H

#include "../vm/yapl_vm.h"
#include "yapl_scanner.h"

/* Compiler operations */
ObjFunction *compile(VM *vm, const char *source);

/* Compilation error messages */
/* Expressions */
#define GRP_EXPR_ERR  "Expected ')' after expression."
#define EXPR_ERR      "Expected expression."
#define EXPR_STMT_ERR "Expected a ';' after expression."

/* Conditionals and Loops */
#define IF_STMT_ERR       "Expected a '{' after 'if' condition."
#define SWITCH_STMT_ERR   "Expected a '{' before switch cases."
#define ELSE_END_ERR      "Cases or else are not allowed after an else case."
#define ARR_CASE_ERR      "Expected a '->' after case."
#define ARR_ELSE_ERR      "Expected a '->' after else case."
#define STMT_SWITCH_ERR   "Cannot have statements before any switch case."
#define WHILE_STMT_ERR    "Expected a '{' after 'while' condition."
#define FOR_STMT_COND_ERR "Expected a ';' after 'for' loop condition."
#define FOR_STMT_INC_ERR  "Expected a '{' after an increment clause."

/* Variables */
#define RED_INIT_ERR   "Cannot read variable in its own initializer."
#define VAR_REDECL_ERR "Variable or closure already declared in this scope."
#define INV_ASSG_ERR   "Invalid assignment target."
#define VAR_NAME_ERR   "Expected a variable name."
#define VAR_DECL_ERR   "Expected a ';' after variable declaration."

/* Functions and Blocks */
#define BLOCK_BRACE_ERR      "Expected a '}' after block."
#define CALL_LIST_PAREN_ERR  "Expected a ')' after arguments."
#define FUNC_NAME_PAREN_ERR  "Expected a '(' after function name."
#define FUNC_LIST_PAREN_ERR  "Expected a ')' after parameters."
#define FUNC_BODY_BRACE_ERR  "Expected a '{' before function body."
#define FUNC_NAME_ERR        "Expected a function name."
#define PARAM_NAME_ERR       "Expected a parameter name."
#define RETURN_STMT_ERR      "Expected a ';' after return value."
#define RETURN_TOP_LEVEL_ERR "Cannot return from top level code."

/* Limits */
#define CONST_LIMIT_ERR   "Limit of 255 constants reached."
#define LOOP_LIMIT_ERR    "Limit of 65535 instructions in loop body reached."
#define JUMP_LIMIT_ERR    "Limit of 65535 instructions in conditional branch reached."
#define VAR_LIMIT_ERR     "Limit of 255 local variables in scope reached."
#define CLOSURE_LIMIT_ERR "Limit of 255 closure variables reached."
#define ARGS_LIMIT_ERR    "Limit of 255 arguments reached."
#define PARAMS_LIMIT_ERR  "Limit of 255 parameters reached."

#endif // YAPL_COMPILER_H
