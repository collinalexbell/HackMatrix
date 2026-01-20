#ifndef UTIL_MEM_H
#define UTIL_MEM_H

#include <stdbool.h>
#include <stddef.h>

/**
 * Allocate a new block of memory and copy *src to it, then store the address
 * of the new allocation in *out.  Returns true if it worked, or false if
 * allocation failed.
 */
bool memdup(void *out, const void *src, size_t size);

#endif // UTIL_MEM_H
