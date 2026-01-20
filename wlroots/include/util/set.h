#ifndef UTIL_SET_H
#define UTIL_SET_H

#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>

/**
 * Add target to values.
 *
 * Target is added to the end of the set.
 *
 * Returns the index of target, or -1 if the set is full or target already
 * exists.
 */
ssize_t set_add(uint32_t values[], size_t *len, size_t cap, uint32_t target);

/**
 * Remove target from values.
 *
 * When target is removed, the last element of the set is moved to where
 * target was.
 *
 * Returns the previous index of target, or -1 if target wasn't in values.
 */
ssize_t set_remove(uint32_t values[], size_t *len, size_t cap, uint32_t target);

#endif

