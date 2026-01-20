/*
 * This an unstable interface of wlroots. No guarantees are made regarding the
 * future consistency of this API.
 */
#ifndef WLR_USE_UNSTABLE
#error "Add -DWLR_USE_UNSTABLE to enable unstable wlroots features"
#endif

#ifndef WLR_UTIL_ADDON_H
#define WLR_UTIL_ADDON_H

#include <wayland-server-core.h>

struct wlr_addon_set {
	struct {
		struct wl_list addons;
	} WLR_PRIVATE;
};

struct wlr_addon;

struct wlr_addon_interface {
	const char *name;
	// Has to call wlr_addon_finish()
	void (*destroy)(struct wlr_addon *addon);
};

struct wlr_addon {
	const struct wlr_addon_interface *impl;

	struct {
		const void *owner;
		struct wl_list link;
	} WLR_PRIVATE;
};

void wlr_addon_set_init(struct wlr_addon_set *set);
void wlr_addon_set_finish(struct wlr_addon_set *set);

void wlr_addon_init(struct wlr_addon *addon, struct wlr_addon_set *set,
	const void *owner, const struct wlr_addon_interface *impl);
void wlr_addon_finish(struct wlr_addon *addon);

struct wlr_addon *wlr_addon_find(struct wlr_addon_set *set, const void *owner,
	const struct wlr_addon_interface *impl);

#endif
