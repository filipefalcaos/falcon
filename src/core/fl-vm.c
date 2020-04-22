/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * fl-vm.c: Falcon's stack-based virtual machine
 * See Falcon's license in the LICENSE file
 */

#include "fl-vm.h"
#include "../lib/fl-baselib.h"
#include "../lib/fl-iolib.h"
#include "../lib/fl-mathlib.h"
#include "../lib/fl-strlib.h"
#include "../lib/fl-syslib.h"
#include "fl-debug.h"
#include "fl-mem.h"
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/**
 * Prints to stderr a stack trace of call frames from a given initial one to a given final one.
 */
static void print_call_frames(FalconVM *vm, int initial, int final) {
    for (int i = initial; i >= final; i--) {
        CallFrame *currentFrame = &vm->frames[i];
        ObjFunction *currentFunction = currentFrame->closure->function;
        size_t currentInstruction = currentFrame->pc - currentFunction->bytecode.code - 1;
        int currentLine = get_source_line(&currentFunction->bytecode, (int) currentInstruction);

        /* Prints line and function name */
        fprintf(stderr, "    [Line %d] in ", currentLine);
        if (currentFunction->name == NULL) {
            fprintf(stderr, "%s\n", FALCON_SCRIPT);
        } else {
            fprintf(stderr, "%s()\n", currentFunction->name->chars);
        }
    }
}

/**
 * Prints a runtime error to stderr based on the given format and args. The error message also
 * includes a stack trace (last call first).
 */
static void runtime_error(FalconVM *vm, const char *format, va_list args) {
    fprintf(stderr, "RuntimeError: ");
    vfprintf(stderr, format, args); /* Prints the error */
    fprintf(stderr, "\n");

    /* Prints a stack trace */
    fprintf(stderr, "Stack trace (last call first):\n");
    if (vm->frameCount > FALCON_MAX_TRACE) {
        print_call_frames(vm, vm->frameCount - 1, vm->frameCount - (FALCON_MAX_TRACE / 2));
        fprintf(stderr, "    ...\n");
        print_call_frames(vm, (FALCON_MAX_TRACE / 2) - 1, 0);
        fprintf(stderr, "%d call frames not listed. Run with option \"--debug\" to see all.\n",
                vm->frameCount - FALCON_MAX_TRACE);
    } else {
        print_call_frames(vm, vm->frameCount - 1, 0);
    }
}

/**
 * Prints a runtime error to stderr and resets the virtual machine stack. The error message is
 * composed of a given format, followed by the arguments of that format.
 */
void interpreter_error(FalconVM *vm, const char *format, ...) {
    va_list args;
    va_start(args, format);
    runtime_error(vm, format, args); /* Presents the error */
    va_end(args);
    reset_stack(vm); /* Resets the stack due to error */
}

/**
 * Resets the Falcon's virtual machine stack.
 */
void reset_stack(FalconVM *vm) {
    vm->stackTop = vm->stack;
    vm->openUpvalues = NULL;
    vm->frameCount = 0;
}

/* Native functions implementations. It is composed by a name (string) and a function
 * (FalconNativeFn implementation) */
static const ObjNative NATIVE_FUNCTIONS[] = {

    /* Base lib */
    {.function = lib_print, .name = "print"},
    {.function = lib_type, .name = "type"},
    {.function = lib_bool, .name = "bool"},
    {.function = lib_num, .name = "num"},
    {.function = lib_str, .name = "str"},
    {.function = lib_len, .name = "len"},
    {.function = lib_hasField, .name = "hasField"},
    {.function = lib_getField, .name = "getField"},
    {.function = lib_setField, .name = "setField"},
    {.function = lib_delField, .name = "delField"},

    /* System lib */
    {.function = lib_exit, .name = "exit"},
    {.function = lib_clock, .name = "clock"},
    {.function = lib_time, .name = "time"},

    /* Math lib */
    {.function = lib_abs, .name = "abs"},
    {.function = lib_sqrt, .name = "sqrt"},
    {.function = lib_pow, .name = "pow"},

    /* IO lib */
    {.function = lib_input, .name = "input"}

};

/**
 * Defines a new native function for Falcon with a given name and a given implementation.
 */
static void define_native(FalconVM *vm, const char *name, FalconNativeFn function) {
    DISABLE_GC(vm); /* Avoids GC from the "define_native" ahead */
    ObjString *strName = new_ObjString(vm, name, (int) strlen(name));
    ENABLE_GC(vm);
    DISABLE_GC(vm); /* Avoids GC from the "map_set" ahead */
    ObjNative *nativeFn = new_ObjNative(vm, function, name);
    map_set(vm, &vm->globals, strName, OBJ_VAL(nativeFn));
    ENABLE_GC(vm);
}

/**
 * Defines the complete set (see the constant "NATIVE_FUNCTIONS" above) of native functions for
 * Falcon.
 */
static void define_natives(FalconVM *vm) {
    for (unsigned long i = 0; i < sizeof(NATIVE_FUNCTIONS) / sizeof(NATIVE_FUNCTIONS[0]); i++)
        define_native(vm, NATIVE_FUNCTIONS[i].name, NATIVE_FUNCTIONS[i].function);
}

/**
 * Initializes the Falcon's virtual machine.
 */
void init_FalconVM(FalconVM *vm) {
    reset_stack(vm); /* Inits the VM stack */
    vm->fileName = NULL;
    vm->isREPL = false;
    vm->objects = NULL;
    vm->compiler = NULL;

    /* Sets debugging options */
    vm->dumpOpcodes = false;
    vm->traceExec = false;
    vm->traceMemory = false;

    /* Inits the garbage collection fields */
    vm->gcEnabled = true;
    vm->grayCount = 0;
    vm->grayCapacity = 0;
    vm->grayStack = NULL;
    vm->bytesAllocated = 0;
    vm->nextGC = VM_BASE_HEAP_SIZE;

    vm->initStr = NULL;
    vm->strings = *new_ObjMap(vm);              /* Inits the map of interned strings */
    vm->globals = *new_ObjMap(vm);              /* Inits the map of globals */
    vm->initStr = new_ObjString(vm, "init", 4); /* Defines the string for class initializers */
    define_natives(vm);                         /* Sets native functions */
}

/**
 * Frees the Falcon's virtual machine and its allocated objects.
 */
void free_FalconVM(FalconVM *vm) {
    vm->initStr = NULL;
    free_map(vm, &vm->strings);
    free_map(vm, &vm->globals);
    free_vm_objects(vm);
}

/**
 * Pushes a value to the top of the Falcon's virtual machine stack.
 */
bool push(FalconVM *vm, FalconValue value) {
    if ((vm->stackTop - &vm->stack[0]) > FALCON_STACK_MAX - 1) {
        interpreter_error(vm, VM_STACK_OVERFLOW);
        return false;
    }

    *vm->stackTop = value;
    vm->stackTop++;
    return true;
}

/**
 * Pops a value from the top of the Falcon's virtual machine stack by decreasing the "stackTop"
 * pointer.
 */
FalconValue pop(FalconVM *vm) {
    vm->stackTop--;
    return *vm->stackTop;
}

/**
 * Peeks a element on the Falcon's virtual machine stack.
 */
static FalconValue peek(FalconVM *vm, int distance) { return vm->stackTop[-1 - distance]; }

/**
 * Executes a call on the given Falcon function by setting its call frame to be run. If the call
 * succeeds, "true" is returned, and otherwise, "false".
 */
static bool call(FalconVM *vm, ObjClosure *closure, int argCount) {
    if (argCount != closure->function->arity) {
        interpreter_error(vm, VM_ARGS_COUNT_ERR, closure->function->arity, argCount);
        return false;
    }

    if (vm->frameCount == FALCON_FRAMES_MAX) {
        interpreter_error(vm, VM_STACK_OVERFLOW);
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
 * otherwise, "false". If the value is not callable, a runtime error message is also presented.
 */
static bool call_value(FalconVM *vm, FalconValue callee, int argCount) {
    if (IS_OBJ(callee)) {
        switch (OBJ_TYPE(callee)) {
            case OBJ_CLASS: {
                FalconValue init;
                ObjClass *class_ = AS_CLASS(callee);
                vm->stackTop[-argCount - 1] = OBJ_VAL(new_ObjInstance(vm, class_));

                /* Calls the class "init" method, if existent */
                if (map_get(&class_->methods, vm->initStr, &init)) {
                    return call(vm, AS_CLOSURE(init), argCount); /* Calls "init" as a closure */
                } else if (argCount != 0) {
                    interpreter_error(vm, VM_INIT_ERR, argCount);
                    return false;
                }

                return true;
            }
            case OBJ_BMETHOD: {
                ObjBMethod *bMethod = AS_BMETHOD(callee);
                vm->stackTop[-argCount - 1] = bMethod->receiver; /* Set the "this" bound receiver */
                return call(vm, bMethod->method, argCount);      /* Calls the method as a closure */
            }
            case OBJ_CLOSURE:
                return call(vm, AS_CLOSURE(callee), argCount);
            case OBJ_NATIVE: {
                FalconNativeFn native = AS_NATIVE(callee)->function;
                FalconValue out = native(vm, argCount, vm->stackTop - argCount);
                if (IS_ERR(out)) return false;    /* Checks if a runtime error occurred */
                vm->stackTop -= argCount + 1;     /* Updates the stack to where it was */
                if (!push(vm, out)) return false; /* Pushes the return value */
                return true;
            }
            default:
                break; /* Not callable */
        }
    }

    interpreter_error(vm, VM_VALUE_NOT_CALL_ERR);
    return false;
}

/**
 * Tries to bind a method to the receiver (i.e., a class instance), on the top of the stack. If the
 * binding succeeds, "true" is returned, and otherwise, "false". If the method does not exist, a
 * runtime error message is also presented.
 */
static bool bind_method(FalconVM *vm, ObjClass *class_, ObjString *methodName) {
    FalconValue method;
    if (!map_get(&class_->methods, methodName, &method)) { /* Checks if the method is defined */
        interpreter_error(vm, VM_UNDEF_PROP_ERR, class_->name->chars, methodName->chars);
        return false;
    }

    /* Binds the method to the receiver */
    ObjBMethod *bMethod = new_ObjBMethod(vm, peek(vm, 0), AS_CLOSURE(method));
    pop(vm);
    push(vm, OBJ_VAL(bMethod));
    return true;
}

/**
 * Tries to invoke a given method of a class instance. If the invocation succeeds, "true" is
 * returned, and otherwise, "false". If the method does not exist, a runtime error message is also
 * presented.
 */
static bool invoke_from_class(FalconVM *vm, ObjClass *class_, ObjString *methodName, int argCount) {
    FalconValue method;
    if (!map_get(&class_->methods, methodName, &method)) { /* Checks if method is defined */
        interpreter_error(vm, VM_UNDEF_PROP_ERR);
        return false;
    }

    return call(vm, AS_CLOSURE(method), argCount); /* Calls the method as a closure */
}

/**
 * Tries to invoke a given property of a class instance. If the invocation succeeds, "true" is
 * returned, and otherwise, "false". If the receiver is not a class instance, a runtime error
 * message is also presented.
 */
static bool invoke(FalconVM *vm, ObjString *calleeName, int argCount) {
    FalconValue receiver = peek(vm, argCount);
    if (!IS_INSTANCE(receiver)) {
        interpreter_error(vm, VM_NOT_INSTANCE_ERR);
        return false;
    }

    ObjInstance *instance = AS_INSTANCE(receiver);
    FalconValue property;
    if (map_get(&instance->fields, calleeName, &property)) { /* Checks if is shadowed by a field */
        vm->stackTop[-argCount - 1] = property;
        return call_value(vm, property, argCount); /* Tries to execute the field as a function */
    }

    return invoke_from_class(vm, instance->class_, calleeName, argCount); /* Invokes the method */
}

/**
 * Captures a given local variable into a upvalue. If the local was already captured as an upvalue,
 * this upvalue is returned. Otherwise, a new upvalue is created and added to the list of open
 * upvalues in the virtual machine.
 */
static ObjUpvalue *capture_upvalue(FalconVM *vm, FalconValue *local) {
    ObjUpvalue *prevUpvalue = NULL;
    ObjUpvalue *upvalue = vm->openUpvalues;

    /* Iterate past upvalues pointing to slots above the given one */
    while (upvalue != NULL && upvalue->slot > local) {
        prevUpvalue = upvalue;
        upvalue = upvalue->next;
    }

    if (upvalue != NULL && upvalue->slot == local) /* Checks if already exists in the list */
        return upvalue;

    ObjUpvalue *createdUpvalue = new_ObjUpvalue(vm, local); /* Creates a new upvalue */
    createdUpvalue->next = upvalue;                         /* Adds to the list */

    if (prevUpvalue == NULL) {
        vm->openUpvalues = createdUpvalue;
    } else {
        prevUpvalue->next = createdUpvalue;
    }

    return createdUpvalue;
}

/**
 * Closes the open upvalues created for the virtual machine stack slots that are above a given slot
 * (including the slot itself).
 */
static void close_upvalues(FalconVM *vm, FalconValue *last) {
    while (vm->openUpvalues != NULL && vm->openUpvalues->slot >= last) {
        ObjUpvalue *upvalue = vm->openUpvalues;
        upvalue->closed = *upvalue->slot;
        upvalue->slot = &upvalue->closed;
        vm->openUpvalues = upvalue->next;
    }
}

/**
 * Defines a new class method by adding it to the map of methods in a class.
 */
static void define_method(FalconVM *vm, ObjString *name) {
    FalconValue method = pop(vm);
    ObjClass *class_ = AS_CLASS(peek(vm, 0));
    DISABLE_GC(vm);                              /* Avoids GC from the "map_set" ahead */
    map_set(vm, &class_->methods, name, method); /* Sets the new method */
    DISABLE_GC(vm);
}

/**
 * Compares the two string values on the top of the virtual machine stack. If the two strings are
 * equal, returns 0. If the first string is lexicographically smaller, returns a negative integer.
 * Otherwise, returns a positive one.
 */
static int compare_strings(FalconVM *vm) {
    ObjString *str2 = AS_STRING(pop(vm));
    return cmp_strings(AS_STRING(vm->stackTop[-1]), str2);
}

/**
 * Concatenates the two string values on the top of the virtual machine stack. Then, pushes the new
 * string to the stack.
 */
static void concatenate_strings(FalconVM *vm) {
    ObjString *b = AS_STRING(pop(vm));
    ObjString *a = AS_STRING(pop(vm));

    DISABLE_GC(vm);                               /* Avoids GC from the "concat_strings" ahead */
    ObjString *result = concat_strings(vm, b, a); /* Concatenates both strings */
    ENABLE_GC(vm);
    push(vm, OBJ_VAL(result));                   /* Pushes concatenated string */
    map_set(vm, &vm->strings, result, NULL_VAL); /* Interns the string */
}

/**
 * The main bytecode interpreter loop. It loops through all the instructions in the bytecode chunks
 * of the running functions. Each turn through the loop, it reads and executes the current
 * instruction.
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

/* Checks if a given Value is numerical. If not, a runtime error is returned */
#define ASSERT_NUM(vm, value, error)      \
    do {                                  \
        if (!IS_NUM(value)) {             \
            interpreter_error(vm, error); \
            return FALCON_RUNTIME_ERROR;  \
        }                                 \
    } while (false)

/* Checks if a given Value is a string. If not, a runtime error is returned */
#define ASSERT_STR(vm, value, error)      \
    do {                                  \
        if (!IS_STRING(value)) {          \
            interpreter_error(vm, error); \
            return FALCON_RUNTIME_ERROR;  \
        }                                 \
    } while (false)

/* Checks if a given Value is numerical and its value is also different from zero. If not, a
 * runtime error is returned */
#define ASSERT_NOT_0(vm, value, error)              \
    do {                                            \
        if (!IS_NUM(value) || AS_NUM(value) == 0) { \
            interpreter_error(vm, error);           \
            return FALCON_RUNTIME_ERROR;            \
        }                                           \
    } while (false)

/* Performs a binary operation of a given C's "op" operator on the two elements on the top of the
 * Falcon VM's stack. Then, sets the result on the top of the stack */
#define BINARY_OP(vm, op, valueType)                     \
    do {                                                 \
        ASSERT_NUM(vm, peek(vm, 0), VM_OPR_NOT_NUM_ERR); \
        ASSERT_NUM(vm, peek(vm, 1), VM_OPR_NOT_NUM_ERR); \
        double b = AS_NUM(pop(vm));                      \
        double a = AS_NUM((vm)->stackTop[-1]);           \
        (vm)->stackTop[-1] = valueType(a op b);          \
    } while (false)

/* Performs a division operation (integer mod or double division) on the two elements on the top of
 * the Falcon VM's stack. Then, sets the result on the top of the stack */
#define DIVISION_OP(vm, op, type)                        \
    do {                                                 \
        ASSERT_NOT_0(vm, peek(vm, 0), VM_DIV_ZERO_ERR);  \
        ASSERT_NUM(vm, peek(vm, 1), VM_OPR_NOT_NUM_ERR); \
        type b = AS_NUM(pop(vm));                        \
        type a = AS_NUM((vm)->stackTop[-1]);             \
        (vm)->stackTop[-1] = NUM_VAL(a op b);            \
    } while (false)

/* Performs a greater/less (GL) comparison operation of a given C's "op" operator on the two
 * elements on the top of the Falcon VM's stack. Then, sets the result on the top of the stack */
#define GL_COMPARE(vm, op)                                                             \
    do {                                                                               \
        if (IS_STRING(peek(vm, 0)) && IS_STRING(peek(vm, 1))) {                        \
            int comparison = compare_strings(vm);                                      \
            (vm)->stackTop[-1] = (comparison op 0) ? BOOL_VAL(true) : BOOL_VAL(false); \
        } else if (IS_NUM(peek(vm, 0)) && IS_NUM(peek(vm, 1))) {                       \
            double a = AS_NUM(pop(vm));                                                \
            (vm)->stackTop[-1] = BOOL_VAL(AS_NUM((vm)->stackTop[-1]) op a);            \
        } else {                                                                       \
            interpreter_error(vm, VM_OPR_NOT_NUM_STR_ERR);                             \
            return FALCON_RUNTIME_ERROR;                                               \
        }                                                                              \
    } while (false)

    if (vm->traceExec) {
        if (vm->dumpOpcodes || vm->traceMemory) printf("\n");
        PRINT_TRACE_HEADER();
    }

    /* Main bytecode interpreter loop */
    while (true) {
        if (vm->traceExec) { /* Prints the execution trace if the option "-t" is set */
            trace_execution(vm, frame);
        }

        uint8_t instruction = READ_BYTE();
        switch (instruction) { /* Reads the next byte and switches through the opcodes */

            /* Constants and literals */
            case OP_LOADCONST: {
                uint16_t index = READ_BYTE() | (uint16_t)(READ_BYTE() << 8u);
                if (!push(vm, CURR_CONSTANTS().values[index])) return FALCON_RUNTIME_ERROR;
                break;
            }
            case OP_LOADFALSE:
                if (!push(vm, BOOL_VAL(false))) return FALCON_RUNTIME_ERROR;
                break;
            case OP_LOADTRUE:
                if (!push(vm, BOOL_VAL(true))) return FALCON_RUNTIME_ERROR;
                break;
            case OP_LOADNULL:
                if (!push(vm, NULL_VAL)) return FALCON_RUNTIME_ERROR;
                break;

            /* Lists */
            case OP_DEFLIST: {
                uint16_t elementsCount = READ_SHORT();
                ObjList *list = new_ObjList(vm, elementsCount);

                /* Adds the elements to the list */
                for (uint16_t i = 0; i < elementsCount; i++) {
                    list->elements.values[i] = peek(vm, elementsCount - i - 1);
                }

                for (uint16_t i = 0; i < elementsCount; i++) pop(vm); /* Discards the elements */
                push(vm, OBJ_VAL(list));
                break;
            }
            case OP_DEFMAP: {
                uint16_t entriesCount = READ_SHORT();
                ObjMap *map = new_ObjMap(vm);

                /* Adds the entries to the map */
                for (uint16_t i = 0; i < entriesCount; i++) {
                    FalconValue key = peek(vm, 1);
                    ASSERT_STR(vm, key, VM_MAP_INDEX_ERR);
                    FalconValue value = peek(vm, 0);
                    map_set(vm, map, AS_STRING(key), value);

                    /* Discards the entry's key and value */
                    pop(vm);
                    pop(vm);
                }

                push(vm, OBJ_VAL(map));
                break;
            }
            case OP_GETSUB: {
                FalconValue index = pop(vm);
                FalconValue subscript = pop(vm);

                if (!IS_OBJ(subscript)) { /* Checks if subscript is an object */
                    interpreter_error(vm, VM_INDEX_ERR);
                    return FALCON_RUNTIME_ERROR;
                }

                /* Handles the subscript types */
                switch (AS_OBJ(subscript)->type) {
                    case OBJ_LIST: {
                        ASSERT_NUM(vm, index, VM_LIST_INDEX_ERR);
                        int listIndex = AS_NUM(index);
                        ObjList *list = AS_LIST(subscript);

                        if (listIndex < 0) listIndex = list->elements.count + listIndex;
                        if (listIndex >= 0 && listIndex < list->elements.count) {
                            push(vm, list->elements.values[listIndex]);
                            break;
                        }

                        /* Out of bounds index */
                        interpreter_error(vm, VM_LIST_BOUNDS_ERR);
                        return FALCON_RUNTIME_ERROR;
                    }
                    case OBJ_MAP: {
                        ASSERT_STR(vm, index, VM_MAP_INDEX_ERR);
                        ObjString *string = AS_STRING(index);
                        ObjMap *map = AS_MAP(subscript);

                        FalconValue value;
                        if (!map_get(map, string, &value)) { /* Checks if key exist */
                            push(vm, NULL_VAL);
                        } else {
                            push(vm, value);
                        }

                        break;
                    }
                    case OBJ_STRING: {
                        ASSERT_NUM(vm, index, VM_STRING_INDEX_ERR);
                        int strIndex = AS_NUM(index);
                        ObjString *string = AS_STRING(subscript);

                        if (strIndex < 0) strIndex = (int) string->length + strIndex;
                        if (strIndex >= 0 && strIndex < string->length) {
                            push(vm, OBJ_VAL(new_ObjString(vm, &string->chars[strIndex], 1)));
                            break;
                        }

                        /* Out of bounds index */
                        interpreter_error(vm, VM_STRING_BOUNDS_ERR);
                        return FALCON_RUNTIME_ERROR;
                    }
                    default: /* Only lists and strings can be subscript */
                        interpreter_error(vm, VM_INDEX_ERR);
                        return FALCON_RUNTIME_ERROR;
                }

                break;
            }
            case OP_SETSUB: {
                FalconValue value = pop(vm);
                FalconValue index = pop(vm);
                FalconValue subscript = pop(vm);

                if (!IS_OBJ(subscript)) { /* Checks if subscript is an object */
                    interpreter_error(vm, VM_INDEX_ERR);
                    return FALCON_RUNTIME_ERROR;
                }

                switch (AS_OBJ(subscript)->type) { /* Handles the subscript types */
                    case OBJ_LIST: {
                        ASSERT_NUM(vm, index, VM_LIST_INDEX_ERR);
                        int listIndex = AS_NUM(index);
                        ObjList *list = AS_LIST(subscript);

                        if (listIndex < 0) listIndex = list->elements.count + listIndex;
                        if (listIndex >= 0 && listIndex < list->elements.count) {
                            list->elements.values[listIndex] = value;
                            push(vm, value);
                            break;
                        }

                        /* Out of bounds index */
                        interpreter_error(vm, VM_LIST_BOUNDS_ERR);
                        return FALCON_RUNTIME_ERROR;
                    }
                    case OBJ_MAP: {
                        ASSERT_STR(vm, index, VM_MAP_INDEX_ERR);
                        ObjString *key = AS_STRING(index);
                        ObjMap *map = AS_MAP(subscript);
                        map_set(vm, map, key, value);
                        push(vm, value);
                        break;
                    }
                    case OBJ_STRING: {
                        interpreter_error(vm, VM_STRING_MUT_ERR);
                        return FALCON_RUNTIME_ERROR;
                    }
                    default: /* Only lists and maps support subscript assignment */
                        interpreter_error(vm, VM_INDEX_ASSG_ERR);
                        return FALCON_RUNTIME_ERROR;
                }

                break;
            }

            /* Relational operations */
            case OP_AND: {
                uint16_t offset = READ_SHORT();

                if (is_falsy(peek(vm, 0))) {
                    frame->pc += offset;
                } else {
                    pop(vm);
                }

                break;
            }
            case OP_OR: {
                uint16_t offset = READ_SHORT();

                if (is_falsy(peek(vm, 0))) {
                    pop(vm);
                } else {
                    frame->pc += offset;
                }

                break;
            }
            case OP_NOT:
                vm->stackTop[-1] = BOOL_VAL(is_falsy(vm->stackTop[-1]));
                break;
            case OP_EQUAL: {
                FalconValue b = pop(vm);
                vm->stackTop[-1] = BOOL_VAL(values_equal(vm->stackTop[-1], b));
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
                if (IS_STRING(peek(vm, 0)) && IS_STRING(peek(vm, 1))) {
                    concatenate_strings(vm);
                } else if (IS_NUM(peek(vm, 0)) && IS_NUM(peek(vm, 1))) {
                    double a = AS_NUM(pop(vm));
                    vm->stackTop[-1] = NUM_VAL(AS_NUM(vm->stackTop[-1]) + a);
                } else {
                    interpreter_error(vm, VM_OPR_NOT_NUM_STR_ERR);
                    return FALCON_RUNTIME_ERROR;
                }

                break;
            }
            case OP_SUB:
                BINARY_OP(vm, -, NUM_VAL);
                break;
            case OP_NEG:
                ASSERT_NUM(vm, peek(vm, 0), VM_OPR_NOT_NUM_ERR);
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
                ASSERT_NUM(vm, peek(vm, 0), VM_OPR_NOT_NUM_ERR);
                ASSERT_NUM(vm, peek(vm, 1), VM_OPR_NOT_NUM_ERR);
                double a = AS_NUM(pop(vm));
                vm->stackTop[-1] = NUM_VAL(pow(AS_NUM(vm->stackTop[-1]), a));
                break;
            }

            /* Variable operations */
            case OP_DEFGLOBAL: {
                ObjString *name = READ_STRING();
                map_set(vm, &vm->globals, name, peek(vm, 0));
                pop(vm);
                break;
            }
            case OP_GETGLOBAL: {
                ObjString *name = READ_STRING();
                FalconValue value;

                if (!map_get(&vm->globals, name, &value)) { /* Checks if undefined */
                    interpreter_error(vm, VM_UNDEF_VAR_ERR, (name)->chars);
                    return FALCON_RUNTIME_ERROR;
                }

                if (!push(vm, value)) return FALCON_RUNTIME_ERROR;
                break;
            }
            case OP_SETGLOBAL: {
                ObjString *name = READ_STRING();
                if (map_set(vm, &vm->globals, name, peek(vm, 0))) { /* Checks if undefined */
                    map_remove(&vm->globals, name);
                    interpreter_error(vm, VM_UNDEF_VAR_ERR, (name)->chars);
                    return FALCON_RUNTIME_ERROR;
                }

                break;
            }
            case OP_GETUPVAL: {
                uint8_t slot = READ_BYTE();
                if (!push(vm, *frame->closure->upvalues[slot]->slot)) return FALCON_RUNTIME_ERROR;
                break;
            }
            case OP_SETUPVAL: {
                uint8_t slot = READ_BYTE();
                *frame->closure->upvalues[slot]->slot = peek(vm, 0);
                break;
            }
            case OP_CLOSEUPVAL:
                close_upvalues(vm, vm->stackTop - 1);
                pop(vm);
                break;
            case OP_GETLOCAL: {
                uint8_t slot = READ_BYTE();
                if (!push(vm, frame->slots[slot])) return FALCON_RUNTIME_ERROR;
                break;
            }
            case OP_SETLOCAL: {
                uint8_t slot = READ_BYTE();
                frame->slots[slot] = peek(vm, 0);
                break;
            }

            /* Jump/loop operations */
            case OP_JUMP: {
                uint16_t offset = READ_SHORT();
                frame->pc += offset;
                break;
            }
            case OP_JUMPIFF: {
                uint16_t offset = READ_SHORT();
                if (is_falsy(peek(vm, 0))) frame->pc += offset;
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
                ObjClosure *closure = new_ObjClosure(vm, function);
                if (!push(vm, OBJ_VAL(closure))) return FALCON_RUNTIME_ERROR;

                /* Capture upvalues */
                for (int i = 0; i < closure->upvalueCount; i++) {
                    uint8_t isLocal = READ_BYTE();
                    uint8_t index = READ_BYTE();

                    if (isLocal) {
                        closure->upvalues[i] = capture_upvalue(vm, frame->slots + index);
                    } else {
                        closure->upvalues[i] = frame->closure->upvalues[index];
                    }
                }

                break;
            }
            case OP_CALL: {
                int argCount = READ_BYTE();
                if (!call_value(vm, peek(vm, argCount), argCount)) return FALCON_RUNTIME_ERROR;
                frame = &vm->frames[vm->frameCount - 1]; /* Updates the current frame */
                break;
            }
            case OP_RETURN: {
                FalconValue result = pop(vm);     /* Gets the function's return value */
                close_upvalues(vm, frame->slots); /* Closes upvalues */
                vm->frameCount--;

                if (vm->frameCount == 0) { /* Checks if top level code is finished */
                    pop(vm);               /* Pops "script" from the stack */
                    return FALCON_OK;
                }

                vm->stackTop = frame->slots;                        /* Resets the stack top */
                if (!push(vm, result)) return FALCON_RUNTIME_ERROR; /* Pushes the return value */
                frame = &vm->frames[vm->frameCount - 1];            /* Updates the current frame */
                break;
            }

            /* Class operations */
            case OP_DEFCLASS:
                push(vm, OBJ_VAL(new_ObjClass(vm, READ_STRING())));
                break;
            case OP_INHERIT: {
                FalconValue superclass = peek(vm, 1);
                if (!IS_CLASS(superclass)) { /* Checks if superclass value is valid */
                    interpreter_error(vm, VM_INHERITANCE_ERR);
                    return FALCON_RUNTIME_ERROR;
                }

                /* Applies the inheritance effect */
                ObjClass *subclass = AS_CLASS(peek(vm, 0));
                copy_entries(vm, &AS_CLASS(superclass)->methods, &subclass->methods);
                pop(vm);
                break;
            }
            case OP_DEFMETHOD:
                define_method(vm, READ_STRING());
                break;
            case OP_INVPROP: {
                ObjString *name = READ_STRING();
                int argCount = READ_BYTE();
                if (!invoke(vm, name, argCount)) return FALCON_RUNTIME_ERROR;
                frame = &vm->frames[vm->frameCount - 1]; /* Updates the current frame */
                break;
            }
            case OP_GETPROP: {
                if (!IS_INSTANCE(peek(vm, 0))) {
                    interpreter_error(vm, VM_NOT_INSTANCE_ERR);
                    return FALCON_RUNTIME_ERROR;
                }

                ObjInstance *instance = AS_INSTANCE(peek(vm, 0));
                ObjString *name = READ_STRING();
                FalconValue value;

                /* Looks for a valid field */
                if (map_get(&instance->fields, name, &value)) {
                    pop(vm);         /* Pops the instance */
                    push(vm, value); /* Pushes the field value */
                    break;
                }

                /* Looks for a valid method, leaving it on the stack top */
                if (!bind_method(vm, instance->class_, name))
                    return FALCON_RUNTIME_ERROR; /* Undefined property */

                break;
            }
            case OP_SETPROP: {
                if (!IS_INSTANCE(peek(vm, 1))) {
                    interpreter_error(vm, VM_NOT_INSTANCE_ERR);
                    return FALCON_RUNTIME_ERROR;
                }

                ObjInstance *instance = AS_INSTANCE(peek(vm, 1));
                map_set(vm, &instance->fields, READ_STRING(), peek(vm, 0));

                FalconValue value = pop(vm); /* Pops the assigned value */
                pop(vm);                     /* Pops the instance */
                push(vm, value);             /* Pushes the new field value */
                break;
            }
            case OP_SUPER: {
                ObjString *name = READ_STRING();
                ObjClass *superclass = AS_CLASS(pop(vm));

                /* Tries to look for the method on the superclass, leaving it on the stack top */
                if (!bind_method(vm, superclass, name))
                    return FALCON_RUNTIME_ERROR; /* Undefined superclass method */

                break;
            }
            case OP_INVSUPER: {
                ObjString *name = READ_STRING();
                int argCount = READ_BYTE();
                ObjClass *superclass = AS_CLASS(pop(vm));

                /* Tries to invoke the method from the superclass */
                if (!invoke_from_class(vm, superclass, name, argCount)) return FALCON_RUNTIME_ERROR;
                frame = &vm->frames[vm->frameCount - 1];
                break;
            }

            /* VM operations */
            case OP_DUPT:
                push(vm, peek(vm, 0));
                break;
            case OP_POPT:
                pop(vm);
                break;
            case OP_POPEXPR: {
                FalconValue result = peek(vm, 0);
                if (!IS_NULL(result)) {
                    print_value(vm, result);
                    printf("\n");
                }

                pop(vm);
                break;
            }
            case OP_TEMP:
                interpreter_error(vm, VM_UNREACHABLE_ERR, instruction);
                return FALCON_RUNTIME_ERROR;

            /* Unknown opcode */
            default:
                interpreter_error(vm, VM_UNKNOWN_OPCODE_ERR, instruction);
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
FalconResultCode interpret_source(FalconVM *vm, const char *source) {
    ObjFunction *function = compile_source(vm, source); /* Compiles the source code */
    if (function == NULL) return FALCON_COMPILE_ERROR;

    /* Set the script to run */
    push(vm, OBJ_VAL(function));
    ObjClosure *closure = new_ObjClosure(vm, function);
    pop(vm);
    push(vm, OBJ_VAL(closure));
    call_value(vm, OBJ_VAL(closure), 0);

    return run(vm); /* Executes the bytecode chunk */
}
