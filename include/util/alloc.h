#ifndef UTIL_ALLOC_H_
#define UTIL_ALLOC_H_

#include <stdbool.h>
#include <stddef.h>

/*
 * Allocates len bytes of heap memory and stores the resulting pointer
 * in *out.
 *
 * len: number of bytes to allocate. Must be greater than zero.
 *
 * out: non-NULL pointer to a pointer where the allocated address will
 *      be stored. On failure, *out is left unchanged.
 *
 * Returns true if the allocation succeeds, or false if len is zero,
 * out is NULL, or the allocation fails.
 *
 * Ownership: the caller owns the allocated memory and must release it
 * with owsgFree() when it is no longer needed.
 */
bool owsgAllocImpl(size_t len, void **out);

/*
 * Frees heap memory previously allocated by owsgAlloc() and sets the
 * caller's pointer to NULL.
 *
 * ptr: non-NULL pointer to the pointer being freed. If ptr or *ptr is
 *      NULL, this function does nothing.
 *
 * The pointer must refer to memory allocated by owsgAlloc() or a
 * compatible allocation that can safely be passed to free().
 */
void owsgFreeImpl(void **ptr);

#define owsgAlloc(len, out) owsgAllocImpl((len), (void **)(out))
#define owsgFree(ptr) owsgFreeImpl((void **)(ptr))

#endif /* UTIL_ALLOC_H_ */
