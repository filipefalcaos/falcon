/*
 * YAPL version 0.0.1 (master, Oct 18 2019)
 * yapl_memory_manager.h: YAPL's automatic memory manager
 * See YAPL's license in the LICENSE file
 */

#ifndef YAPL_MEMORY_MANAGER_H
#define YAPL_MEMORY_MANAGER_H

/* Doubles the capacity of a given dynamic array based on its current one */
#define INCREASE_CAPACITY(capacity) ((capacity) < 8 ? 8 : (capacity) * 2)

/* Increases the allocation of a given dynamic array */
#define INCREASE_ARRAY(previous, type, oldCount, count) \
    (type *) reallocate(previous, sizeof(type) * (oldCount), sizeof(type) * (count))

/* Frees the memory allocated on a dynamic array by passing in zero for the new size to the
 * "reallocate" function */
#define FREE_ARRAY(type, pointer, oldCount) reallocate(pointer, sizeof(type) * (oldCount), 0)

/* Memory management operations */
void *reallocate(void *previous, size_t oldSize, size_t newSize);

#endif // YAPL_MEMORY_MANAGER_H
