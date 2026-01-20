#ifndef TYPES_WLR_REGION_H
#define TYPES_WLR_REGION_H

#include <stdint.h>

struct wl_client;

/*
 * Creates a new region resource with the provided new ID.
 */
struct wl_resource *region_create(struct wl_client *client,
	uint32_t version, uint32_t id);

#endif
