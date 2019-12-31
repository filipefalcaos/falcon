/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * falcon_vm.c: Falcon's stack-based virtual machine
 * See Falcon's license in the LICENSE file
 */

#include "falcon_vm.h"
#include "../compiler/falcon_compiler.h"
#include "../lib/falcon_error.h"
#include "../lib/falcon_natives.h"
#include "../lib/falcon_string.h"
#include "falcon_memory.h"
#include <math.h>
#include <stdio.h>

#ifdef FALCON_DEBUG_LEVEL_01
#include "falcon_debug.h"
#endif

/**
 * Resets the virtual machine stack.
 */
void resetVMStack(FalconVM *vm) {
    vm->stackTop = vm->stack;
    vm->openUpvalues = NULL;
    vm->frameCount = 0;
}

/**
 * Initializes the Falcon's virtual machine.
 */
void falconInitVM(FalconVM *vm) {
    resetVMStack(vm); /* Inits the VM stack */

    /* Inits the VM fields */
    vm->fileName = NULL;
    vm->isREPL = false;
    vm->objects = NULL;
    vm->compiler = NULL;

    /* Inits the garbage collection fields */
    vm->grayCount = 0;
    vm->grayCapacity = 0;
    vm->grayStack = NULL;
    vm->bytesAllocated = 0;
    vm->nextGC = VM_BASE_HEAP_SIZE;

    initTable(&vm->strings); /* Inits the table of interned strings */
    initTable(&vm->globals); /* Inits the table of globals */
    falconDefNatives(vm);          /* Sets native functions */
}

/**
 * Frees the Falcon's virtual machine and its allocated objects.
 */
void falconFreeVM(FalconVM *vm) {
    freeTable(vm, &vm->strings);
    freeTable(vm, &vm->globals);
    falconFreeObjs(vm);
}

/**
 * Pushes a value to the Falcon's virtual machine stack.
 */
bool VMPush(FalconVM *vm, FalconValue value) {
    if ((vm->stackTop - &vm->stack[0]) > FALCON_STACK_MAX - 1) {
        falconVMError(vm, VM_STACK_OVERFLOW);
        return false;
    }

    *vm->stackTop = value;
    vm->stackTop++;
    return true;
}

/**
 * Pops a value from the Falcon's virtual machine stack.
 */
FalconValue VMPop(FalconVM *vm) {
    vm->stackTop--;
    return *vm->stackTop;
}

/**
 * Peeks a element on the Falcon's virtual machine stack.
 */
static FalconValue peek(FalconVM *vm, int distance) { return vm->stackTop[-1 - distance]; }

/**
 * Executes a call on the given Falcon function by setting its call frame to be run.
 */
static bool call(FalconVM *vm, ObjClosure *closure, int argCount) {
    if (argCount != closure->function->arity) {
        falconVMError(vm, VM_ARGS_COUNT_ERR, closure->function->arity, argCount);
        return false;
    }

    if (vm->frameCount == FALCON_FRAMES_MAX) {
        falconVMError(vm, VM_STACK_OVERFLOW);
        return false;
    }

    CallFrame *frame = &vm->frames[vm->frameCount++];
    frame->closure = closure;
    frame->pc = closure->function->bytecode.code;
    frame->slots = vm->stackTop - argCount - 1;
    return true;
}

/**
 * Tries to execute a function call on a given Falcon value.
 */
static bool callValue(FalconVM *vm, FalconValue callee, int argCount) {
    if (IS_OBJ(callee)) {
        switch (OBJ_TYPE(callee)) {
            case OBJ_CLOSURE:
                return call(vm, AS_CLOSURE(callee), argCount);
            case OBJ_NATIVE: {
                FalconNativeFn native = AS_NATIVE(callee)->function;
                FalconValue out =
                    native(vm, argCount, vm->stackTop - argCount); /* Runs native function */
                if (IS_ERR(out)) return false;   /* Checks if a runtime error occurred */
                vm->stackTop -= argCount + 1;           /* Updates the stack to where it was */
                if (!VMPush(vm, out)) return false; /* Pushes the return value */
                return true;
            }
            default:
                break; /* Not callable */
        }
    }

    falconVMError(vm, VM_VALUE_NOT_CALL_ERR);
    return false;
}

/**
 * Captures a given local upvalue.
 */
static ObjUpvalue *captureUpvalue(FalconVM *vm, FalconValue *local) {
    ObjUpvalue *prevUpvalue = NULL;
    ObjUpvalue *upvalue = vm->openUpvalues;

    /* Iterate past upvalues pointing to slots above the given one */
    while (upvalue != NULL && upvalue->slot > local) {
        prevUpvalue = upvalue;
        upvalue = upvalue->next;
    }

    if (upvalue != NULL && upvalue->slot == local) /* Checks if already exists in the list */
        return upvalue;

    ObjUpvalue *createdUpvalue = falconUpvalue(vm, local); /* Creates a new upvalue */
    createdUpvalue->next = upvalue;                        /* Adds to the list */

    if (prevUpvalue == NULL) {
        vm->openUpvalues = createdUpvalue;
    } else {
        prevUpvalue->next = createdUpvalue;
    }

    return createdUpvalue;
}

/**
 * Closes the upvalues for a given stack slot.
 */
static void closeUpvalues(FalconVM *vm, FalconValue *last) {
    while (vm->openUpvalues != NULL && vm->openUpvalues->slot >= last) {
        ObjUpvalue *upvalue = vm->openUpvalues;
        upvalue->closed = *upvalue->slot;
        upvalue->slot = &upvalue->closed;
        vm->openUpvalues = upvalue->next;
    }
}

/**
 * Compares the two string values on the top of the virtual machine stack.
 */
static int compareStrings(FalconVM *vm) {
    ObjString *str2 = AS_STRING(VMPop(vm));
    return cmpStrings(AS_STRING(vm->stackTop[-1]), str2);
}

/**
 * Concatenates the two string values on the top of the virtual machine stack. Then, pushes the new
 * string to the stack.
 */
static void concatenateStrings(FalconVM *vm) {
    ObjString *b = AS_STRING(peek(vm, 0));       /* Avoids GC */
    ObjString *a = AS_STRING(peek(vm, 1));       /* Avoids GC */
    ObjString *result = concatStrings(vm, b, a); /* Concatenates both strings */
    VMPop(vm);
    VMPop(vm);
    VMPush(vm, OBJ_VAL(result));                  /* Pushes concatenated string */
    tableSet(vm, &vm->strings, result, NULL_VAL); /* Interns the string */
}

/**
 * Loops through all the instructions in a bytecode chunk. Each turn through the loop, it reads and
 * executes the current instruction.
 */
static FalconResultCode run(FalconVM *vm) {
    CallFrame *frame = &vm->frames[vm->frameCount - 1]; /* Current call frame */

/* Constants of the current running bytecode */
#define CURR_CONSTANTS() frame->closure->function->bytecode.constants

/* Reads the next 8 bits (byte) or 16 bits (2 bytes) */
#define READ_BYTE()  (*frame->pc++)
#define READ_SHORT() (frame->pc += 2, ((uint16_t)(frame->pc[-2] << 8u) | frame->pc[-1]))

/* Reads the next byte from the bytecode, treats the resulting number as an index, and looks up the
 * corresponding location in the chunk’s constant table */
#define READ_CONSTANT() CURR_CONSTANTS().values[READ_BYTE()]
#define READ_STRING() AS_STRING(READ_CONSTANT())

/* Checks if the two values at the top of the Falcon VM's stack are numerical Values. If not, a
 * runtime error is returned */
#define ASSERT_TOP2_NUM(vm)                                               \
    do {                                                                  \
        if (!IS_NUM(peek(vm, 0)) || !IS_NUM(peek(vm, 1))) { \
            falconVMError(vm, VM_OPR_NOT_NUM_ERR);                    \
            return FALCON_RUNTIME_ERROR;                                  \
        }                                                                 \
    } while (false)

/* Checks if the value at the given position "pos" of the Falcon VM's stack is a numerical Value.
 * If not, a runtime error is returned */
#define ASSERT_NUM(vm, pos, error)           \
    do {                                     \
        if (!IS_NUM(peek(vm, pos))) { \
            falconVMError(vm, error);        \
            return FALCON_RUNTIME_ERROR;     \
        }                                    \
    } while (false)

/* Checks if the value at the given position "pos" of the Falcon VM's stack is not zero. If not, a
 * runtime error is returned */
#define ASSERT_NOT_0(vm, pos)                       \
    do {                                            \
        if (AS_NUM(peek(vm, pos)) == 0) {    \
            falconVMError(vm, VM_DIV_ZERO_ERR); \
            return FALCON_RUNTIME_ERROR;            \
        }                                           \
    } while (false)

/* Performs a binary operation of the "op" operator on the two elements on the top of the Falcon
 * VM's stack. Then, sets the result on the top of the stack */
#define BINARY_OP(vm, op, valueType)                  \
    do {                                              \
        ASSERT_TOP2_NUM(vm);                          \
        double b = AS_NUM(VMPop(vm));      \
        double a = AS_NUM((vm)->stackTop[-1]); \
        (vm)->stackTop[-1] = valueType(a op b);       \
    } while (false)

/* Performs a division operation (integer mod or double division) on the two elements on the top of
 * the Falcon VM's stack. Then, sets the result on the top of the stack */
#define DIVISION_OP(vm, op, type)                    \
    do {                                             \
        ASSERT_TOP2_NUM(vm);                         \
        ASSERT_NOT_0(vm, 0);                         \
        type b = AS_NUM(VMPop(vm));       \
        type a = AS_NUM((vm)->stackTop[-1]);  \
        (vm)->stackTop[-1] = NUM_VAL(a op b); \
    } while (false)

/* Performs a greater/less (GL) comparison operation of the "op" operator on the two elements on
 * the top of the Falcon VM's stack. Then, sets the result on the top of the stack */
#define GL_COMPARE(vm, op)                                                                \
    do {                                                                                  \
        if (IS_STRING(peek(vm, 0)) && IS_STRING(peek(vm, 1))) {             \
            int comparison = compareStrings(vm);                                          \
            (vm)->stackTop[-1] =                                                          \
                (comparison op 0) ? BOOL_VAL(true) : BOOL_VAL(false);       \
        } else if (IS_NUM(peek(vm, 0)) && IS_NUM(peek(vm, 1))) {            \
            double a = AS_NUM(VMPop(vm));                                      \
            (vm)->stackTop[-1] = BOOL_VAL(AS_NUM((vm)->stackTop[-1]) op a); \
        } else {                                                                          \
            falconVMError(vm, VM_OPR_NOT_NUM_STR_ERR);                                \
            return FALCON_RUNTIME_ERROR;                                                  \
        }                                                                                 \
    } while (false)

/* Presents a runtime error when trying to access an index that is out of bounds */
#define OUT_OF_BOUNDS_ERR(vm, error) \
    do {                             \
        falconVMError(vm, error);    \
        return FALCON_RUNTIME_ERROR; \
    } while (false)

/* Presents a runtime error when a global variable is undefined */
#define UNDEF_VAR_ERR(vm, name, delete)                         \
    do {                                                        \
        if (delete) tableDelete(&(vm)->globals, name);    \
        falconVMError(vm, VM_UNDEF_VAR_ERR, (name)->chars); \
        return FALCON_RUNTIME_ERROR;                            \
    } while (false)

    /* Main virtual machine loop */
    while (true) {
#ifdef FALCON_DEBUG_LEVEL_01
        if (vm->stack != vm->stackTop) dumpStack(vm);
        dumpInstruction(vm, &frame->closure->function->bytecode,
                        (int) (frame->pc - frame->closure->function->bytecode.code));
#endif

        uint8_t instruction = READ_BYTE();
        switch (instruction) { /* Reads the next byte and switches through the opcodes */

            /* Constants and literals */
            case LOAD_CONST: {
                uint16_t index = READ_BYTE() | (uint16_t)(READ_BYTE() << 8u);
                if (!VMPush(vm, CURR_CONSTANTS().values[index])) return FALCON_RUNTIME_ERROR;
                break;
            }
            case LOAD_FALSE:
                if (!VMPush(vm, BOOL_VAL(false))) return FALCON_RUNTIME_ERROR;
                break;
            case LOAD_TRUE:
                if (!VMPush(vm, BOOL_VAL(true))) return FALCON_RUNTIME_ERROR;
                break;
            case LOAD_NULL:
                if (!VMPush(vm, NULL_VAL)) return FALCON_RUNTIME_ERROR;
                break;

            /* Lists */
            case DEF_LIST: {
                ObjList *list = falconList(vm, READ_BYTE());
                VMPush(vm, OBJ_VAL(list));
                break;
            }
            case PUSH_LIST: {
                FalconValue element = peek(vm, 0); /* Avoids GC */
                ObjList *list = AS_LIST(peek(vm, 1));
                writeValArray(vm, &list->elements, element);
                VMPop(vm);
                break;
            }
            case GET_SUBSCRIPT: {
                ASSERT_NUM(vm, 0, VM_INDEX_NOT_NUM_ERR); /* Checks if index is valid */
                int index = (int) AS_NUM(VMPop(vm));
                FalconValue subscript = VMPop(vm);

                if (!IS_OBJ(subscript)) { /* Checks if subscript is an object */
                    falconVMError(vm, VM_INDEX_ERR);
                    return FALCON_RUNTIME_ERROR;
                }

                switch (AS_OBJ(subscript)->type) { /* Handles the subscript types */
                    case OBJ_LIST: {
                        ObjList *list = AS_LIST(subscript);
                        if (index < 0) index = list->elements.count + index;
                        if (index >= 0 && index < list->elements.count) {
                            VMPush(vm, list->elements.values[index]);
                            break;
                        }

                        OUT_OF_BOUNDS_ERR(vm, VM_LIST_BOUNDS_ERR); /* Out of bounds index */
                    }
                    case OBJ_STRING: {
                        ObjString *string = AS_STRING(subscript);
                        if (index < 0) index = (int) string->length + index;
                        if (index >= 0 && index < string->length) {
                            VMPush(vm, OBJ_VAL(copyString(vm, &string->chars[index], 1)));
                            break;
                        }

                        OUT_OF_BOUNDS_ERR(vm, VM_STRING_BOUNDS_ERR); /* Out of bounds index */
                    }
                    default: /* Only lists and strings can be subscript */
                        falconVMError(vm, VM_INDEX_ERR);
                        return FALCON_RUNTIME_ERROR;
                }

                break;
            }
            case SET_SUBSCRIPT: {
                ASSERT_NUM(vm, 1, VM_INDEX_NOT_NUM_ERR); /* Checks if index is valid */
                FalconValue value = VMPop(vm);
                int index = (int) AS_NUM(VMPop(vm));
                FalconValue subscript = VMPop(vm);

                if (!IS_OBJ(subscript)) { /* Checks if subscript is an object */
                    falconVMError(vm, VM_INDEX_ERR);
                    return FALCON_RUNTIME_ERROR;
                }

                switch (AS_OBJ(subscript)->type) { /* Handles the subscript types */
                    case OBJ_LIST: {
                        ObjList *list = AS_LIST(subscript);
                        if (index < 0) index = list->elements.count + index;
                        if (index >= 0 && index < list->elements.count) {
                            list->elements.values[index] = value;
                            VMPush(vm, value);
                            break;
                        }

                        OUT_OF_BOUNDS_ERR(vm, VM_LIST_BOUNDS_ERR); /* Out of bounds index */
                    }
                    default: /* Only lists support subscript assignment */
                        falconVMError(vm, VM_INDEX_ASSG_ERR);
                        return FALCON_RUNTIME_ERROR;

                }

                break;
            }

            /* Relational operations */
            case BIN_AND: {
                uint16_t offset = READ_SHORT();
                if (isFalsy(peek(vm, 0)))
                    frame->pc += offset;
                else
                    VMPop(vm);
                break;
            }
            case BIN_OR: {
                uint16_t offset = READ_SHORT();
                if (isFalsy(peek(vm, 0)))
                    VMPop(vm);
                else
                    frame->pc += offset;
                break;
            }
            case UN_NOT:
                vm->stackTop[-1] = BOOL_VAL(isFalsy(vm->stackTop[-1]));
                break;
            case BIN_EQUAL: {
                FalconValue b = VMPop(vm);
                vm->stackTop[-1] = BOOL_VAL(valuesEqual(vm->stackTop[-1], b));
                break;
            }
            case BIN_GREATER:
                GL_COMPARE(vm, >);
                break;
            case BIN_LESS:
                GL_COMPARE(vm, <);
                break;

            /* Arithmetic operations */
            case BIN_ADD: {
                if (IS_STRING(peek(vm, 0)) && IS_STRING(peek(vm, 1))) {
                    concatenateStrings(vm);
                } else if (IS_NUM(peek(vm, 0)) && IS_NUM(peek(vm, 1))) {
                    double a = AS_NUM(VMPop(vm));
                    vm->stackTop[-1] = NUM_VAL(AS_NUM(vm->stackTop[-1]) + a);
                } else {
                    falconVMError(vm, VM_OPR_NOT_NUM_STR_ERR);
                    return FALCON_RUNTIME_ERROR;
                }
                break;
            }
            case BIN_SUB:
                BINARY_OP(vm, -, NUM_VAL);
                break;
            case UN_NEG:
                ASSERT_NUM(vm, 0, VM_OPR_NOT_NUM_ERR);
                vm->stackTop[-1] = NUM_VAL(-AS_NUM(vm->stackTop[-1]));
                break;
            case BIN_MULT:
                BINARY_OP(vm, *, NUM_VAL);
                break;
            case BIN_MOD:
                DIVISION_OP(vm, %, int);
                break;
            case BIN_DIV: {
                DIVISION_OP(vm, /, double);
                break;
            }
            case BIN_POW: {
                ASSERT_TOP2_NUM(vm);
                double a = AS_NUM(VMPop(vm));
                vm->stackTop[-1] = NUM_VAL(pow(AS_NUM(vm->stackTop[-1]), a));
                break;
            }

            /* Variable operations */
            case DEF_GLOBAL: {
                ObjString *name = READ_STRING();
                tableSet(vm, &vm->globals, name, peek(vm, 0));
                VMPop(vm);
                break;
            }
            case GET_GLOBAL: {
                ObjString *name = READ_STRING();
                FalconValue value;
                if (!tableGet(&vm->globals, name, &value)) /* Checks if undefined */
                    UNDEF_VAR_ERR(vm, name, false);
                if (!VMPush(vm, value)) return FALCON_RUNTIME_ERROR;
                break;
            }
            case SET_GLOBAL: {
                ObjString *name = READ_STRING();
                if (tableSet(vm, &vm->globals, name, peek(vm, 0))) /* Checks if undefined */
                    UNDEF_VAR_ERR(vm, name, true);
                break;
            }
            case GET_UPVALUE: {
                uint8_t slot = READ_BYTE();
                if (!VMPush(vm, *frame->closure->upvalues[slot]->slot))
                    return FALCON_RUNTIME_ERROR;
                break;
            }
            case SET_UPVALUE: {
                uint8_t slot = READ_BYTE();
                *frame->closure->upvalues[slot]->slot = peek(vm, 0);
                break;
            }
            case CLS_UPVALUE:
                closeUpvalues(vm, vm->stackTop - 1);
                VMPop(vm);
                break;
            case GET_LOCAL: {
                uint8_t slot = READ_BYTE();
                if (!VMPush(vm, frame->slots[slot])) return FALCON_RUNTIME_ERROR;
                break;
            }
            case SET_LOCAL: {
                uint8_t slot = READ_BYTE();
                frame->slots[slot] = peek(vm, 0);
                break;
            }

            /* Jump/loop operations */
            case JUMP_FWR: {
                uint16_t offset = READ_SHORT();
                frame->pc += offset;
                break;
            }
            case JUMP_IF_FALSE: {
                uint16_t offset = READ_SHORT();
                if (isFalsy(peek(vm, 0))) frame->pc += offset;
                break;
            }
            case LOOP_BACK: {
                uint16_t offset = READ_SHORT();
                frame->pc -= offset;
                break;
            }

            /* Function operations */
            case FN_CLOSURE: {
                ObjFunction *function = AS_FUNCTION(READ_CONSTANT());
                ObjClosure *closure = falconClosure(vm, function);
                if (!VMPush(vm, OBJ_VAL(closure))) return FALCON_RUNTIME_ERROR;

                /* Capture upvalues */
                for (int i = 0; i < closure->upvalueCount; i++) {
                    uint8_t isLocal = READ_BYTE();
                    uint8_t index = READ_BYTE();

                    if (isLocal) {
                        closure->upvalues[i] = captureUpvalue(vm, frame->slots + index);
                    } else {
                        closure->upvalues[i] = frame->closure->upvalues[index];
                    }
                }

                break;
            }
            case FN_CALL: {
                int argCount = READ_BYTE();
                if (!callValue(vm, peek(vm, argCount), argCount)) return FALCON_RUNTIME_ERROR;
                frame = &vm->frames[vm->frameCount - 1]; /* Updates the current frame */
                break;
            }
            case FN_RETURN: {
                FalconValue result = VMPop(vm); /* Gets the function's return value */
                closeUpvalues(vm, frame->slots);    /* Closes upvalues */
                vm->frameCount--;

                if (vm->frameCount == 0) { /* Checks if top level code is finished */
                    VMPop(vm);         /* Pops "script" from the stack */
                    return FALCON_OK;
                }

                vm->stackTop = frame->slots; /* Resets the stack top */
                if (!VMPush(vm, result))
                    return FALCON_RUNTIME_ERROR;         /* Pushes the return value */
                frame = &vm->frames[vm->frameCount - 1]; /* Updates the current frame */
                break;
            }

            /* VM operations */
            case DUP_TOP:
                VMPush(vm, peek(vm, 0));
                break;
            case POP_TOP:
                VMPop(vm);
                break;
            case POP_TOP_EXPR: {
                FalconValue result = peek(vm, 0);
                if (!IS_NULL(result)) {
                    printValue(vm, result, true);
                    printf("\n");
                }

                VMPop(vm);
                break;
            }
            case TEMP_MARK:
                falconVMError(vm, VM_UNREACHABLE_ERR, instruction);
                return FALCON_RUNTIME_ERROR;

            /* Unknown opcode */
            default:
                falconVMError(vm, VM_UNKNOWN_OPCODE_ERR, instruction);
                return FALCON_RUNTIME_ERROR;
        }
    }

#undef READ_BYTE
#undef READ_SHORT
#undef READ_CONSTANT
#undef READ_STRING
#undef ASSERT_TOP2_NUM
#undef ASSERT_NUM
#undef ASSERT_NOT_0
#undef ASSERT_SUBSCRIPT
#undef BINARY_OP
#undef DIVISION_OP
#undef GL_COMPARE
}

/**
 * Interprets a Falcon's source code string. If the source is compiled successfully, the bytecode
 * chunk is set for the Falcon's virtual machine to execute.
 */
FalconResultCode falconInterpret(FalconVM *vm, const char *source) {
    ObjFunction *function = falconCompile(vm, source); /* Compiles the source code */
    if (function == NULL) return FALCON_COMPILE_ERROR;

    /* Set the script to run */
    VMPush(vm, OBJ_VAL(function));
    ObjClosure *closure = falconClosure(vm, function);
    VMPop(vm);
    VMPush(vm, OBJ_VAL(closure));
    callValue(vm, OBJ_VAL(closure), 0);

    return run(vm); /* Executes the bytecode chunk */
}
