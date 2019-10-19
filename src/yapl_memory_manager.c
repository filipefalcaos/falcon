/*
 * YAPL version 0.0.1 (master, Oct 18 2019)
 * yapl_memory_manager.c: YAPL's automatic memory manager
 * See YAPL's license in the LICENSE file
 */

#include <stdlib.h>
#include "../include/commons.h"
#include "../include/yapl_memory_manager.h"

/**
 * Handles all dynamic memory management — allocating memory, freeing it, and changing the size of
 * an existing allocation.
 */
void *reallocate(void *previous, size_t oldSize, size_t newSize) {
    if (newSize == 0) {
        free(previous);
        return NULL;
    }

    return realloc(previous, newSize);
}
