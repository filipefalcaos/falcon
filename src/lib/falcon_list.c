/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * falcon_list.c: Falcon's standard list library
 * See Falcon's license in the LICENSE file
 */

#include "falcon_list.h"
#include "../vm/falcon_memory.h"

/**
 * Concatenates two given Falcon lists.
 */
ObjList *concatLists(FalconVM *vm, const ObjList *list1, const ObjList *list2) {
    int length = list1->elements.count + list2->elements.count;
    ObjList *result = FALCON_ALLOCATE_OBJ(vm, ObjList, OBJ_LIST);
    result->elements.count = length;
    result->elements.capacity = length;
    result->elements.values = FALCON_ALLOCATE(vm, FalconValue, length);

    for (int i = 0; i < list2->elements.count; i++) /* Adds "list2" values */
        result->elements.values[i] = list2->elements.values[i];

    for (int i = 0; i < list1->elements.count; i++) /* Adds "list1" values */
        result->elements.values[i + list2->elements.count] = list1->elements.values[i];

    return result;
}
