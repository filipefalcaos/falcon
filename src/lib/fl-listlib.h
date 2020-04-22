/*
 * Falcon version 0.0.2 (master, Dec 02 2019)
 * fl-listlib.h: Falcon's standard list library
 * See Falcon's license in the LICENSE file
 */

#ifndef FL_LISTLIB_H
#define FL_LISTLIB_H

#include "../core/fl-object.h"

/* List functions */
ObjList *concat_lists(FalconVM *vm, const ObjList *list1, const ObjList *list2);
ObjString *list_to_string(FalconVM *vm, ObjList *list);

#endif // FL_LISTLIB_H
