/*
 * AstraOS - Kernel Heap Header
 * Simple block allocator for kmalloc/kfree
 */

#ifndef _ASTRA_MM_HEAP_H
#define _ASTRA_MM_HEAP_H

#include <stdint.h>
#include <stddef.h>

/*
 * Initialize kernel heap
 */
void heap_init(void);

/*
 * Allocate memory
 */
void *kmalloc(size_t size);

/*
 * Allocate zeroed memory
 */
void *kcalloc(size_t count, size_t size);

/*
 * Reallocate memory
 */
void *krealloc(void *ptr, size_t new_size);

/*
 * Free memory
 */
void kfree(void *ptr);

/*
 * Get heap statistics
 */
size_t heap_get_used(void);
size_t heap_get_free(void);

#endif /* _ASTRA_MM_HEAP_H */
