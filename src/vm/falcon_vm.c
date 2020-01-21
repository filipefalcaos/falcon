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
    falconDefNatives(vm);    /* Sets native functions */
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
 * Pops and discard the two value from the top of the Falcon's virtual machine stack.
 */
void VMPop2(FalconVM *vm) {
    VMPop(vm);
    VMPop(vm);
}

/**
 * Pops and discard "n" values from the top of the Falcon's virtual machine stack.
 */
void VMPopN(FalconVM *vm, long int n) {
    for (long int i = 0; i < n; i++) {
        VMPop(vm);
    }
}

/**
 * Peeks a element on the Falcon's virtual machine stack.
 */
static FalconValue VMPeek(FalconVM *vm, int distance) { return vm->stackTop[-1 - distance]; }

/**
 * Executes a call on the given Falcon function by setting its call frame to be run. If the call
 * succeeds, "true" is returned, and otherwise, "false".
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
 * Tries to execute a call on a given Falcon value. If the call succeeds, "true" is returned, and
 * otherwise, "false".
 */
static bool callValue(FalconVM *vm, FalconValue callee, int argCount) {
    if (IS_OBJ(callee)) {
        switch (OBJ_TYPE(callee)) {
            case OBJ_CLASS: {
                ObjClass *class_ = AS_CLASS(callee);
                vm->stackTop[-argCount - 1] = OBJ_VAL(falconInstance(vm, class_));
                return true;
            }
            case OBJ_CLOSURE:
                return call(vm, AS_CLOSURE(callee), argCount);
            case OBJ_NATIVE: {
                FalconNativeFn native = AS_NATIVE(callee)->function;
                FalconValue out =
                    native(vm, argCount, vm->stackTop - argCount); /* Runs native function */
                if (IS_ERR(out)) return false;      /* Checks if a runtime error occurred */
                vm->stackTop -= argCount + 1;       /* Updates the stack to where it was */
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
 * Tries to call a given method of a class instance. If the invocation succeeds, "true" is
 * returned, and otherwise, "false".
 */
static bool callMethod(FalconVM *vm, ObjClass *class_, ObjString *methodName, int argCount) {
    FalconValue method;
    if (!tableGet(&class_->methods, methodName, &method)) { /* Checks if method is defined */
        falconVMError(vm, VM_UNDEF_PROP_ERR);
        return false;
    }

    return call(vm, AS_CLOSURE(method), argCount); /* Calls the method as a closure */
}

/**
 * Tries to invoke a given property of a class instance. If the invocation succeeds, "true" is
 * returned, and otherwise, "false".
 */
static bool invoke(FalconVM *vm, ObjString *calleeName, int argCount) {
    FalconValue receiver = VMPeek(vm, argCount);
    if (!IS_INSTANCE(receiver)) {
        falconVMError(vm, VM_NOT_INSTANCE_ERR);
        return false;
    }

    ObjInstance *instance = AS_INSTANCE(receiver);
    FalconValue property;
    if (tableGet(&instance->fields, calleeName, &property)) { /* Checks if is shadowed by a field */
        vm->stackTop[-argCount - 1] = property;
        return callValue(vm, property, argCount); /* Tries to execute the field as a function */
    }

    return callMethod(vm, instance->class_, calleeName, argCount); /* Invokes the method */
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
 * Defines a new class method by adding it to the table of methods in a class.
 */
static void defineMethod(FalconVM *vm, ObjString *name) {
    FalconValue method = VMPeek(vm, 0);           /* Avoids GC */
    ObjClass *class_ = AS_CLASS(VMPeek(vm, 1));   /* Avoids GC */
    tableSet(vm, &class_->methods, name, method); /* Sets the new method */
    VMPop2(vm);
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
    ObjString *b = AS_STRING(VMPeek(vm, 0));     /* Avoids GC */
    ObjString *a = AS_STRING(VMPeek(vm, 1));     /* Avoids GC */
    ObjString *result = concatStrings(vm, b, a); /* Concatenates both strings */
    VMPop2(vm);
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
#define READ_STRING()   AS_STRING(READ_CONSTANT())

/* Checks if the two values at the top of the Falcon VM's stack are numerical Values. If not, a
 * runtime error is returned */
#define ASSERT_TOP2_NUM(vm)                                     \
    do {                                                        \
        if (!IS_NUM(VMPeek(vm, 0)) || !IS_NUM(VMPeek(vm, 1))) { \
            falconVMError(vm, VM_OPR_NOT_NUM_ERR);              \
            return FALCON_RUNTIME_ERROR;                        \
        }                                                       \
    } while (false)

/* Checks if the value at the given position "pos" of the Falcon VM's stack is a numerical Value.
 * If not, a runtime error is returned */
#define ASSERT_NUM(vm, pos, error)       \
    do {                                 \
        if (!IS_NUM(VMPeek(vm, pos))) {  \
            falconVMError(vm, error);    \
            return FALCON_RUNTIME_ERROR; \
        }                                \
    } while (false)

/* Checks if the value at the given position "pos" of the Falcon VM's stack is not zero. If not, a
 * runtime error is returned */
#define ASSERT_NOT_0(vm, pos)                   \
    do {                                        \
        if (AS_NUM(VMPeek(vm, pos)) == 0) {     \
            falconVMError(vm, VM_DIV_ZERO_ERR); \
            return FALCON_RUNTIME_ERROR;        \
        }                                       \
    } while (false)

/* Performs a binary operation of the "op" operator on the two elements on the top of the Falcon
 * VM's stack. Then, sets the result on the top of the stack */
#define BINARY_OP(vm, op, valueType)            \
    do {                                        \
        ASSERT_TOP2_NUM(vm);                    \
        double b = AS_NUM(VMPop(vm));           \
        double a = AS_NUM((vm)->stackTop[-1]);  \
        (vm)->stackTop[-1] = valueType(a op b); \
    } while (false)

/* Performs a division operation (integer mod or double division) on the two elements on the top of
 * the Falcon VM's stack. Then, sets the result on the top of the stack */
#define DIVISION_OP(vm, op, type)             \
    do {                                      \
        ASSERT_TOP2_NUM(vm);                  \
        ASSERT_NOT_0(vm, 0);                  \
        type b = AS_NUM(VMPop(vm));           \
        type a = AS_NUM((vm)->stackTop[-1]);  \
        (vm)->stackTop[-1] = NUM_VAL(a op b); \
    } while (false)

/* Performs a greater/less (GL) comparison operation of the "op" operator on the two elements on
 * the top of the Falcon VM's stack. Then, sets the result on the top of the stack */
#define GL_COMPARE(vm, op)                                                             \
    do {                                                                               \
        if (IS_STRING(VMPeek(vm, 0)) && IS_STRING(VMPeek(vm, 1))) {                    \
            int comparison = compareStrings(vm);                                       \
            (vm)->stackTop[-1] = (comparison op 0) ? BOOL_VAL(true) : BOOL_VAL(false); \
        } else if (IS_NUM(VMPeek(vm, 0)) && IS_NUM(VMPeek(vm, 1))) {                   \
            double a = AS_NUM(VMPop(vm));                                              \
            (vm)->stackTop[-1] = BOOL_VAL(AS_NUM((vm)->stackTop[-1]) op a);            \
        } else {                                                                       \
            falconVMError(vm, VM_OPR_NOT_NUM_STR_ERR);                                 \
            return FALCON_RUNTIME_ERROR;                                               \
        }                                                                              \
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
            case OP_LOADCONST: {
                uint16_t index = READ_BYTE() | (uint16_t)(READ_BYTE() << 8u);
                if (!VMPush(vm, CURR_CONSTANTS().values[index])) return FALCON_RUNTIME_ERROR;
                break;
            }
            case OP_LOADFALSE:
                if (!VMPush(vm, BOOL_VAL(false))) return FALCON_RUNTIME_ERROR;
                break;
            case OP_LOADTRUE:
                if (!VMPush(vm, BOOL_VAL(true))) return FALCON_RUNTIME_ERROR;
                break;
            case OP_LOADNULL:
                if (!VMPush(vm, NULL_VAL)) return FALCON_RUNTIME_ERROR;
                break;

            /* Lists */
            case OP_DEFLIST: {
                uint16_t elementsCount = READ_SHORT();
                ObjList list = *falconList(vm, elementsCount);

                /* Adds the elements to the list */
                for (uint16_t i = 0; i < elementsCount; i++) {
                    list.elements.values[i] = VMPeek(vm, elementsCount - i - 1);
                }

                VMPopN(vm, elementsCount); /* Discards the elements */
                VMPush(vm, OBJ_VAL(&list));
                break;
            }
            case OP_GETSUB: {
                ASSERT_NUM(vm, 0, VM_INDEX_NOT_NUM_ERR); /* Checks if index is valid */
                int index = (int) AS_NUM(VMPop(vm));
                FalconValue subscript = VMPop(vm);

                if (!IS_OBJ(subscript)) { /* Checks if subscript is an object */
                    falconVMError(vm, VM_INDEX_ERR);
                    return FALCON_RUNTIME_ERROR;
                }

                /* Handles the subscript types */
                switch (AS_OBJ(subscript)->type) {
                    case OBJ_LIST: {
                        ObjList *list = AS_LIST(subscript);
                        if (index < 0) index = list->elements.count + index;
                        if (index >= 0 && index < list->elements.count) {
                            VMPush(vm, list->elements.values[index]);
                            break;
                        }

                        /* Out of bounds index */
                        falconVMError(vm, VM_LIST_BOUNDS_ERR);
                        return FALCON_RUNTIME_ERROR;
                    }
                    case OBJ_STRING: {
                        ObjString *string = AS_STRING(subscript);
                        if (index < 0) index = (int) string->length + index;
                        if (index >= 0 && index < string->length) {
                            VMPush(vm, OBJ_VAL(falconString(vm, &string->chars[index], 1)));
                            break;
                        }

                        /* Out of bounds index */
                        falconVMError(vm, VM_STRING_BOUNDS_ERR);
                        return FALCON_RUNTIME_ERROR;
                    }
                    default: /* Only lists and strings can be subscript */
                        falconVMError(vm, VM_INDEX_ERR);
                        return FALCON_RUNTIME_ERROR;
                }

                break;
            }
            case OP_SETSUB: {
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

                        /* Out of bounds index */
                        falconVMError(vm, VM_LIST_BOUNDS_ERR);
                        return FALCON_RUNTIME_ERROR;
                    }
                    default: /* Only lists support subscript assignment */
                        falconVMError(vm, VM_INDEX_ASSG_ERR);
                        return FALCON_RUNTIME_ERROR;
                }

                break;
            }

            /* Relational operations */
            case OP_AND: {
                uint16_t offset = READ_SHORT();
                if (isFalsy(VMPeek(vm, 0)))
                    frame->pc += offset;
                else
                    VMPop(vm);
                break;
            }
            case OP_OR: {
                uint16_t offset = READ_SHORT();
                if (isFalsy(VMPeek(vm, 0)))
                    VMPop(vm);
                else
                    frame->pc += offset;
                break;
            }
            case OP_NOT:
                vm->stackTop[-1] = BOOL_VAL(isFalsy(vm->stackTop[-1]));
                break;
            case OP_EQUAL: {
                FalconValue b = VMPop(vm);
                vm->stackTop[-1] = BOOL_VAL(valuesEqual(vm->stackTop[-1], b));
                break;
            }
            case OP_GREATER:
                GL_COMPARE(vm, >);
                break;
            case OP_LESS:
                GL_COMPARE(vm, <);
                break;

            /* Arithmetic operations */
            case OP_ADD: {
                if (IS_STRING(VMPeek(vm, 0)) && IS_STRING(VMPeek(vm, 1))) {
                    concatenateStrings(vm);
                } else if (IS_NUM(VMPeek(vm, 0)) && IS_NUM(VMPeek(vm, 1))) {
                    double a = AS_NUM(VMPop(vm));
                    vm->stackTop[-1] = NUM_VAL(AS_NUM(vm->stackTop[-1]) + a);
                } else {
                    falconVMError(vm, VM_OPR_NOT_NUM_STR_ERR);
                    return FALCON_RUNTIME_ERROR;
                }

                break;
            }
            case OP_SUB:
                BINARY_OP(vm, -, NUM_VAL);
                break;
            case OP_NEG:
                ASSERT_NUM(vm, 0, VM_OPR_NOT_NUM_ERR);
                vm->stackTop[-1] = NUM_VAL(-AS_NUM(vm->stackTop[-1]));
                break;
            case OP_MULT:
                BINARY_OP(vm, *, NUM_VAL);
                break;
            case OP_MOD:
                DIVISION_OP(vm, %, int);
                break;
            case OP_DIV: {
                DIVISION_OP(vm, /, double);
                break;
            }
            case OP_POW: {
                ASSERT_TOP2_NUM(vm);
                double a = AS_NUM(VMPop(vm));
                vm->stackTop[-1] = NUM_VAL(pow(AS_NUM(vm->stackTop[-1]), a));
                break;
            }

            /* Variable operations */
            case OP_DEFGLOBAL: {
                ObjString *name = READ_STRING();
                tableSet(vm, &vm->globals, name, VMPeek(vm, 0));
                VMPop(vm);
                break;
            }
            case OP_GETGLOBAL: {
                ObjString *name = READ_STRING();
                FalconValue value;

                if (!tableGet(&vm->globals, name, &value)) { /* Checks if undefined */
                    falconVMError(vm, VM_UNDEF_VAR_ERR, (name)->chars);
                    return FALCON_RUNTIME_ERROR;
                }

                if (!VMPush(vm, value)) return FALCON_RUNTIME_ERROR;
                break;
            }
            case OP_SETGLOBAL: {
                ObjString *name = READ_STRING();
                if (tableSet(vm, &vm->globals, name, VMPeek(vm, 0))) { /* Checks if undefined */
                    tableDelete(&(vm)->globals, name);
                    falconVMError(vm, VM_UNDEF_VAR_ERR, (name)->chars);
                    return FALCON_RUNTIME_ERROR;
                }

                break;
            }
            case OP_GETUPVAL: {
                uint8_t slot = READ_BYTE();
                if (!VMPush(vm, *frame->closure->upvalues[slot]->slot)) return FALCON_RUNTIME_ERROR;
                break;
            }
            case OP_SETUPVAL: {
                uint8_t slot = READ_BYTE();
                *frame->closure->upvalues[slot]->slot = VMPeek(vm, 0);
                break;
            }
            case OP_CLOSEUPVAL:
                closeUpvalues(vm, vm->stackTop - 1);
                VMPop(vm);
                break;
            case OP_GETLOCAL: {
                uint8_t slot = READ_BYTE();
                if (!VMPush(vm, frame->slots[slot])) return FALCON_RUNTIME_ERROR;
                break;
            }
            case OP_SETLOCAL: {
                uint8_t slot = READ_BYTE();
                frame->slots[slot] = VMPeek(vm, 0);
                break;
            }

            /* Jump/loop operations */
            case OP_JUMP: {
                uint16_t offset = READ_SHORT();
                frame->pc += offset;
                break;
            }
            case OP_JUMPIFFALSE: {
                uint16_t offset = READ_SHORT();
                if (isFalsy(VMPeek(vm, 0))) frame->pc += offset;
                break;
            }
            case OP_LOOP: {
                uint16_t offset = READ_SHORT();
                frame->pc -= offset;
                break;
            }

            /* Function operations */
            case OP_CLOSURE: {
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
            case OP_CALL: {
                int argCount = READ_BYTE();
                if (!callValue(vm, VMPeek(vm, argCount), argCount)) return FALCON_RUNTIME_ERROR;
                frame = &vm->frames[vm->frameCount - 1]; /* Updates the current frame */
                break;
            }
            case OP_RETURN: {
                FalconValue result = VMPop(vm);  /* Gets the function's return value */
                closeUpvalues(vm, frame->slots); /* Closes upvalues */
                vm->frameCount--;

                if (vm->frameCount == 0) { /* Checks if top level code is finished */
                    VMPop(vm);             /* Pops "script" from the stack */
                    return FALCON_OK;
                }

                vm->stackTop = frame->slots;                          /* Resets the stack top */
                if (!VMPush(vm, result)) return FALCON_RUNTIME_ERROR; /* Pushes the return value */
                frame = &vm->frames[vm->frameCount - 1]; /* Updates the current frame */
                break;
            }

            /* Class operations */
            case OP_DEFCLASS:
                VMPush(vm, OBJ_VAL(falconClass(vm, READ_STRING())));
                break;
            case OP_DEFMETHOD:
                defineMethod(vm, READ_STRING());
                break;
            case OP_GETPROP: {
                if (!IS_INSTANCE(VMPeek(vm, 0))) {
                    falconVMError(vm, VM_NOT_INSTANCE_ERR);
                    return FALCON_RUNTIME_ERROR;
                }

                ObjInstance *instance = AS_INSTANCE(VMPeek(vm, 0));
                ObjString *name = READ_STRING();
                FalconValue value;

                if (tableGet(&instance->fields, name, &value)) {
                    VMPop(vm);         /* Pops the instance */
                    VMPush(vm, value); /* Pushes the field value */
                    break;
                }

                /* Undefined field error */
                falconVMError(vm, VM_UNDEF_PROP_ERR, instance->class_->name->chars, name->chars);
                return FALCON_RUNTIME_ERROR;
            }
            case OP_SETPROP: {
                if (!IS_INSTANCE(VMPeek(vm, 1))) {
                    falconVMError(vm, VM_NOT_INSTANCE_ERR);
                    return FALCON_RUNTIME_ERROR;
                }

                ObjInstance *instance = AS_INSTANCE(VMPeek(vm, 1));
                tableSet(vm, &instance->fields, READ_STRING(), VMPeek(vm, 0));

                FalconValue value = VMPop(vm); /* Pops the assigned value */
                VMPop(vm);                     /* Pops the instance */
                VMPush(vm, value);             /* Pushes the new field value */
                break;
            }
            case OP_INVPROP: {
                int argCount = READ_BYTE();
                if (!invoke(vm, READ_STRING(), argCount)) return FALCON_RUNTIME_ERROR;
                frame = &vm->frames[vm->frameCount - 1]; /* Updates the current frame */
                break;
            }

            /* VM operations */
            case OP_DUPTOP:
                VMPush(vm, VMPeek(vm, 0));
                break;
            case OP_POPTOP:
                VMPop(vm);
                break;
            case OP_POPTOPEXPR: {
                FalconValue result = VMPeek(vm, 0);
                if (!IS_NULL(result)) {
                    printValue(vm, result);
                    printf("\n");
                }

                VMPop(vm);
                break;
            }
            case OP_TEMP:
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
