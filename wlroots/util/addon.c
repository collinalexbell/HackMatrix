#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-server-core.h>
#include <wlr/util/addon.h>
#include <wlr/util/log.h>

void wlr_addon_set_init(struct wlr_addon_set *set) {
	*set = (struct wlr_addon_set){0};
	wl_list_init(&set->addons);
}

void wlr_addon_set_finish(struct wlr_addon_set *set) {
	while (!wl_list_empty(&set->addons)) {
		struct wl_list *link = set->addons.next;
		struct wlr_addon *addon = wl_container_of(link, addon, link);
		const struct wlr_addon_interface *impl = addon->impl;
		addon->impl->destroy(addon);
		if (set->addons.next == link) {
			wlr_log(WLR_ERROR, "Dangling addon: %s", impl->name);
			abort();
		}
	}
}

void wlr_addon_init(struct wlr_addon *addon, struct wlr_addon_set *set,
		const void *owner, const struct wlr_addon_interface *impl) {
	assert(impl);
	*addon = (struct wlr_addon){
		.impl = impl,
		.owner = owner,
	};
	struct wlr_addon *iter;
	wl_list_for_each(iter, &set->addons, link) {
		if (iter->owner == addon->owner && iter->impl == addon->impl) {
			assert(0 && "Can't have two addons of the same type with the same owner");
		}
	}
	wl_list_insert(&set->addons, &addon->link);
}

void wlr_addon_finish(struct wlr_addon *addon) {
	wl_list_remove(&addon->link);
}

struct wlr_addon *wlr_addon_find(struct wlr_addon_set *set, const void *owner,
		const struct wlr_addon_interface *impl) {
	struct wlr_addon *addon;
	wl_list_for_each(addon, &set->addons, link) {
		if (addon->owner == owner && addon->impl == impl) {
			return addon;
		}
	}
	return NULL;
}
