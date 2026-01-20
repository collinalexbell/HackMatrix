#include <assert.h>
#include <string.h>
#include <wayland-server-protocol.h>
#include <wlr/interfaces/wlr_buffer.h>
#include <wlr/util/log.h>
#include "types/wlr_buffer.h"

/* struct wlr_buffer_resource_interface */
static struct wl_array buffer_resource_interfaces = {0};

void wlr_buffer_register_resource_interface(
		const struct wlr_buffer_resource_interface *iface) {
	assert(iface);
	assert(iface->is_instance);
	assert(iface->from_resource);

	const struct wlr_buffer_resource_interface **iface_ptr;
	wl_array_for_each(iface_ptr, &buffer_resource_interfaces) {
		if (*iface_ptr == iface) {
			wlr_log(WLR_DEBUG, "wlr_resource_buffer_interface %s has already"
					"been registered", iface->name);
			return;
		}
	}

	iface_ptr = wl_array_add(&buffer_resource_interfaces, sizeof(iface));
	*iface_ptr = iface;
}

static const struct wlr_buffer_resource_interface *get_buffer_resource_iface(
		struct wl_resource *resource) {
	struct wlr_buffer_resource_interface **iface_ptr;
	wl_array_for_each(iface_ptr, &buffer_resource_interfaces) {
		if ((*iface_ptr)->is_instance(resource)) {
			return *iface_ptr;
		}
	}

	return NULL;
}

struct wlr_buffer *wlr_buffer_try_from_resource(struct wl_resource *resource) {
	if (strcmp(wl_resource_get_class(resource), wl_buffer_interface.name) != 0) {
		return NULL;
	}

	const struct wlr_buffer_resource_interface *iface =
			get_buffer_resource_iface(resource);
	if (!iface) {
		wlr_log(WLR_ERROR, "Unknown buffer type");
		return NULL;
	}

	struct wlr_buffer *buffer = iface->from_resource(resource);
	if (!buffer) {
		wlr_log(WLR_ERROR, "Failed to create %s buffer", iface->name);
		return NULL;
	}

	return wlr_buffer_lock(buffer);
}
