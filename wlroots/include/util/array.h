#ifndef UTIL_ARRAY_H
#define UTIL_ARRAY_H

#include <stdlib.h>
#include <stdbool.h>
#include <wayland-util.h>

/**
 * Remove a chunk of memory of the specified size at the specified offset.
 */
void array_remove_at(struct wl_array *arr, size_t offset, size_t size);

/**
 * Grow or shrink the array to fit the specifized size.
 */
bool array_realloc(struct wl_array *arr, size_t size);

#endif
