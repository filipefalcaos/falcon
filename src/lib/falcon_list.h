/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * falcon_list.h: Falcon's standard list library
 * See Falcon's license in the LICENSE file
 */

#ifndef FALCON_LIST_H
#define FALCON_LIST_H

#include "../vm/falcon_object.h"

/* List operations */
ObjList *concatLists(FalconVM *vm, const ObjList *list1, const ObjList *list2);

#endif // FALCON_LIST_H
