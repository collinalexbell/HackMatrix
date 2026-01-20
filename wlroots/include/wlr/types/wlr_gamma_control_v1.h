#ifndef WLR_TYPES_WLR_GAMMA_CONTROL_V1_H
#define WLR_TYPES_WLR_GAMMA_CONTROL_V1_H

#include <wayland-server-core.h>

struct wlr_output;
struct wlr_output_state;

struct wlr_gamma_control_manager_v1 {
	struct wl_global *global;
	struct wl_list controls; // wlr_gamma_control_v1.link

	// Fallback to use when an struct wlr_output doesn't support gamma LUTs.
	// Can be used to apply gamma LUTs via a struct wlr_renderer. Leave zero to
	// indicate that the fallback is unsupported.
	size_t fallback_gamma_size;

	struct {
		struct wl_signal destroy;
		struct wl_signal set_gamma; // struct wlr_gamma_control_manager_v1_set_gamma_event
	} events;

	void *data;

	struct {
		struct wl_listener display_destroy;
	} WLR_PRIVATE;
};

struct wlr_gamma_control_manager_v1_set_gamma_event {
	struct wlr_output *output;
	struct wlr_gamma_control_v1 *control; // may be NULL
};

struct wlr_gamma_control_v1 {
	struct wl_resource *resource;
	struct wlr_output *output;
	struct wlr_gamma_control_manager_v1 *manager;
	struct wl_list link;

	uint16_t *table;
	size_t ramp_size;

	void *data;

	struct {
		struct wl_listener output_destroy_listener;
	} WLR_PRIVATE;
};

struct wlr_gamma_control_manager_v1 *wlr_gamma_control_manager_v1_create(
	struct wl_display *display);
struct wlr_gamma_control_v1 *wlr_gamma_control_manager_v1_get_control(
	struct wlr_gamma_control_manager_v1 *manager, struct wlr_output *output);
bool wlr_gamma_control_v1_apply(struct wlr_gamma_control_v1 *gamma_control,
	struct wlr_output_state *output_state);
struct wlr_color_transform *wlr_gamma_control_v1_get_color_transform(
	struct wlr_gamma_control_v1 *gamma_control);
void wlr_gamma_control_v1_send_failed_and_destroy(struct wlr_gamma_control_v1 *gamma_control);

#endif
