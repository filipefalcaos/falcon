/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * falcon_gc.c: Falcon's garbage collector
 * See Falcon's license in the LICENSE file
 */

#include "falcon_gc.h"
#include "falcon_debug.h"
#include "falcon_memory.h"
#include <stdlib.h>

/* The grow factor for the heap allocation. It follows the logic:
 * - If factor > 1, then the number of bytes required for the next garbage collector will be higher
 * than the current number of allocated bytes;
 * - If factor < 1, then the number of bytes required for the next garbage collector will be lower;
 * - If factor = 1, then the number of bytes required for the next garbage collector will be the
 * current number of allocated bytes */
#define HEAP_GROW_FACTOR 2

/**
 * Marks a FalconObj for garbage collection. Strings and native functions are objects that do not
 * contain any references to trace.
 */
static void markObject(FalconVM *vm, FalconObj *object) {
    if (object == NULL) return;
    if (object->isMarked) return;

    object->isMarked = true;
    if (object->type == OBJ_NATIVE || object->type == OBJ_STRING)
        return; /* Strings and native functions contain no references to trace */

    if (vm->traceMemory) dumpMark(object);
    if (vm->grayCapacity < vm->grayCount + 1) {
        vm->grayCapacity = FALCON_INCREASE_CAPACITY(vm->grayCapacity); /* Increase the capacity */
        vm->grayStack = realloc(vm->grayStack, sizeof(FalconObj *) * vm->grayCapacity);
    }

    vm->grayStack[vm->grayCount++] = object; /* Adds to the GC worklist */
}

/**
 * Marks compilation roots (the ObjFunction the compiler is compiling into) for garbage
 * collection.
 */
static void markCompilerRoots(FalconVM *vm) {
    FunctionCompiler *compiler = vm->compiler;
    while (compiler != NULL) {
        markObject(vm, (FalconObj *) compiler->function);
        compiler = compiler->enclosing;
    }
}

/**
 * Marks a FalconValue for garbage collection.
 */
static void markValue(FalconVM *vm, FalconValue value) {
    if (!IS_OBJ(value)) return; /* Num, bool, and null are not dynamically allocated */
    markObject(vm, AS_OBJ(value));
}

/**
 * Marks every key/value pair in a given hashtable for garbage collection.
 */
static void markTable(FalconVM *vm, Table *table) {
    for (int i = 0; i < table->capacity; i++) {
        Entry *entry = &table->entries[i];
        markObject(vm, (FalconObj *) entry->key);
        markValue(vm, entry->value);
    }
}

/**
 * Marks all FalconValues in a given ValueArray for garbage collection.
 */
static void markArray(FalconVM *vm, ValueArray *array) {
    for (int i = 0; i < array->count; i++) markValue(vm, array->values[i]);
}

/**
 * Marks all the captured upvalues of a given closure for garbage collection.
 */
static void markUpvalues(FalconVM *vm, ObjClosure *closure) {
    for (int i = 0; i < closure->upvalueCount; i++) {
        markObject(vm, (FalconObj *) closure->upvalues[i]);
    }
}

/**
 * Traces all the references of a FalconObj, marking these references "gray", and turning the
 * object "black" for the garbage collection. "Black" objects are the ones with "isMarked" set to
 * true and that are not in the "gray" stack.
 */
static void blackenObject(FalconVM *vm, FalconObj *object) {
    if (vm->traceMemory) dumpBlacken(object);

    switch (object->type) {
        case OBJ_FUNCTION: {
            ObjFunction *function = (ObjFunction *) object;
            markObject(vm, (FalconObj *) function->name);
            markArray(vm, &function->bytecode.constants);
            break;
        }
        case OBJ_UPVALUE: markValue(vm, ((ObjUpvalue *) object)->closed); break;
        case OBJ_CLOSURE: {
            ObjClosure *closure = (ObjClosure *) object;
            markObject(vm, (FalconObj *) closure->function);
            markUpvalues(vm, closure);
            break;
        }
        case OBJ_CLASS: {
            ObjClass *class_ = (ObjClass *) object;
            markObject(vm, (FalconObj *) class_->name);
            markTable(vm, &class_->methods);
            break;
        }
        case OBJ_INSTANCE: {
            ObjInstance *instance = (ObjInstance *) object;
            markObject(vm, (FalconObj *) instance->class_);
            markTable(vm, &instance->fields);
            break;
        }
        case OBJ_BMETHOD: {
            ObjBMethod *bound = (ObjBMethod *) object;
            markValue(vm, bound->receiver);
            markObject(vm, (FalconObj *) bound->method);
            break;
        }
        case OBJ_LIST: {
            ObjList *list = (ObjList *) object;
            markArray(vm, &list->elements);
            break;
        }
        case OBJ_MAP: {
            ObjMap *map = (ObjMap *) object;
            markTable(vm, &map->entries);
            break;
        }
        case OBJ_STRING:
        case OBJ_NATIVE: break;
    }
}

/**
 * Marks all root objects in the virtual machine. Root objects are mostly global variables or
 * objects on the stack.
 */
static void markRoots(FalconVM *vm) {
    for (FalconValue *slot = vm->stack; slot < vm->stackTop; slot++) {
        markValue(vm, *slot); /* Marks stack objects */
    }

    for (int i = 0; i < vm->frameCount; i++) {
        markObject(vm, (FalconObj *) vm->frames[i].closure); /* Marks closure objects */
    }

    for (ObjUpvalue *upvalue = vm->openUpvalues; upvalue != NULL; upvalue = upvalue->next) {
        markObject(vm, (FalconObj *) upvalue); /* Marks open upvalues */
    }

    markTable(vm, &vm->globals);               /* Marks global variables */
    markCompilerRoots(vm);                     /* Marks compilation roots */
    markObject(vm, (FalconObj *) vm->initStr); /* Marks the "init" string */
}

/**
 * Traces the references of the "gray" objects while there are still "gray" objects to trace. After
 * being traced, a "gray" object is turned "black".
 */
static void traceReferences(FalconVM *vm) {
    while (vm->grayCount > 0) {
        FalconObj *object = vm->grayStack[--vm->grayCount];
        blackenObject(vm, object);
    }
}

/**
 * Removes every key/value pair of "white" objects in the hashtable for garbage collection.
 */
static void removeWhitesTable(Table *table) {
    for (int i = 0; i < table->capacity; i++) {
        Entry *current = &table->entries[i];
        if (current->key != NULL && !current->key->obj.isMarked) /* Is a "white" object? */
            tableDelete(table, current->key); /* Removes key/value pair from the table */
    }
}

/**
 * Traverses the list of objects allocated by the virtual machine looking for the "white" ones.
 * When found, "white" objects are removed from the object list and freed.
 */
static void sweep(FalconVM *vm) {
    FalconObj *previous = NULL;
    FalconObj *current = vm->objects;

    while (current != NULL) {
        if (current->isMarked) {       /* Is the object marked? If so, keep it */
            current->isMarked = false; /* Remove the mark for the next GC */
            previous = current;
            current = previous->next;
        } else { /* Unmarked objects - the "white" ones */
            FalconObj *white = current;
            current = current->next;

            /* Removes the "white" object from the list */
            if (previous != NULL) {
                previous->next = current;
            } else {
                vm->objects = current;
            }

            falconFreeObj(vm, white); /* Frees the "white" object */
        }
    }
}

/**
 * Starts an immediate garbage collection procedure to free unused memory. Garbage collection
 * follows the Mark-sweep algorithm. It consists in two steps:
 * - Marking: starting from the roots (any object that the virtual machine can reach directly),
 * trace all of the objects those roots refer to. Each time an object is visited, it is marked;
 * - Sweeping: every object traced is examined: any unmarked object are unreachable, thus garbage,
 * and is freed.
 */
void falconRunGC(FalconVM *vm) {
    size_t bytesAllocated;
    if (vm->traceMemory) {
        dumpGCStatus("Start");
        bytesAllocated = vm->bytesAllocated;
    }

    markRoots(vm);                   /* Marks all roots */
    traceReferences(vm);             /* Traces the references of the "gray" objects */
    removeWhitesTable(&vm->strings); /* Removes the "white" strings from the table */
    sweep(vm);                       /* Reclaim the "white" objects - garbage */
    vm->nextGC = vm->bytesAllocated * HEAP_GROW_FACTOR; /* Adjust the next GC threshold */

    if (vm->traceMemory) {
        dumpGC(vm, bytesAllocated);
        dumpGCStatus("End");
    }
}
