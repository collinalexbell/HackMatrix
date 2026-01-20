#include <assert.h>
#include <limits.h>
#include <pixman.h>
#include <stdbool.h>
#include <stdlib.h>
#include <wayland-server-core.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_pointer_constraints_v1.h>
#include <wlr/util/box.h>
#include <wlr/util/log.h>

#include "pointer-constraints-unstable-v1-protocol.h"

static const struct zwp_locked_pointer_v1_interface locked_pointer_impl;
static const struct zwp_confined_pointer_v1_interface confined_pointer_impl;
static const struct zwp_pointer_constraints_v1_interface pointer_constraints_impl;
static void pointer_constraint_destroy(struct wlr_pointer_constraint_v1 *constraint);

static struct wlr_pointer_constraint_v1 *pointer_constraint_from_resource(
		struct wl_resource *resource) {
	assert(
		wl_resource_instance_of(
			resource, &zwp_confined_pointer_v1_interface,
				&confined_pointer_impl) ||
		wl_resource_instance_of(
			resource, &zwp_locked_pointer_v1_interface,
				&locked_pointer_impl));
	return wl_resource_get_user_data(resource);
}

static struct wlr_pointer_constraints_v1 *pointer_constraints_from_resource(
		struct wl_resource *resource) {
	assert(wl_resource_instance_of(resource, &zwp_pointer_constraints_v1_interface,
		&pointer_constraints_impl));
	return wl_resource_get_user_data(resource);
}

static void resource_destroy(struct wl_client *client,
		struct wl_resource *resource) {
	wl_resource_destroy(resource);
}

static void pointer_constraint_destroy(struct wlr_pointer_constraint_v1 *constraint) {
	if (constraint == NULL || constraint->destroying) {
		return;
	}

	// Calling wlr_pointer_constraint_v1_send_deactivated() for a oneshot constraint
	// that is being destroyed results in another pointer_constraint_destroy() call.
	// Avoid finalizing the state twice by setting a flag.
	constraint->destroying = true;

	wlr_log(WLR_DEBUG, "destroying constraint %p", constraint);

	wl_signal_emit_mutable(&constraint->events.destroy, constraint);

	assert(wl_list_empty(&constraint->events.set_region.listener_list));
	assert(wl_list_empty(&constraint->events.destroy.listener_list));

	wl_resource_set_user_data(constraint->resource, NULL);
	wlr_surface_synced_finish(&constraint->synced);
	wl_list_remove(&constraint->link);
	wl_list_remove(&constraint->surface_destroy.link);
	wl_list_remove(&constraint->seat_destroy.link);
	pixman_region32_fini(&constraint->region);
	free(constraint);
}

static void pointer_constraint_destroy_resource(struct wl_resource *resource) {
	struct wlr_pointer_constraint_v1 *constraint =
		pointer_constraint_from_resource(resource);

	pointer_constraint_destroy(constraint);
}

static void pointer_constraint_handle_set_region(struct wl_client *client,
		struct wl_resource *resource, struct wl_resource *region_resource) {
	struct wlr_pointer_constraint_v1 *constraint =
		pointer_constraint_from_resource(resource);
	if (constraint == NULL) {
		return;
	}

	pixman_region32_clear(&constraint->pending.region);
	if (region_resource) {
		const pixman_region32_t *region = wlr_region_from_resource(region_resource);
		pixman_region32_copy(&constraint->pending.region, region);
	}

	constraint->pending.committed |= WLR_POINTER_CONSTRAINT_V1_STATE_REGION;
}

static void pointer_constraint_set_cursor_position_hint(struct wl_client *client,
		struct wl_resource *resource, wl_fixed_t x, wl_fixed_t y) {
	struct wlr_pointer_constraint_v1 *constraint =
		pointer_constraint_from_resource(resource);
	if (constraint == NULL) {
		return;
	}

	constraint->pending.cursor_hint.enabled = true;
	constraint->pending.cursor_hint.x = wl_fixed_to_double(x);
	constraint->pending.cursor_hint.y = wl_fixed_to_double(y);
	constraint->pending.committed |= WLR_POINTER_CONSTRAINT_V1_STATE_CURSOR_HINT;
}

// Returns true if the region has changed
static bool update_region(struct wlr_pointer_constraint_v1 *constraint) {
	pixman_region32_t region;
	pixman_region32_init(&region);

	if (!pixman_region32_empty(&constraint->current.region)) {
		pixman_region32_intersect(&region,
			&constraint->surface->input_region, &constraint->current.region);
	} else {
		pixman_region32_copy(&region, &constraint->surface->input_region);
	}

	if (pixman_region32_equal(&region, &constraint->region)) {
		pixman_region32_fini(&region);
		return false;
	}

	pixman_region32_fini(&constraint->region);
	constraint->region = region;

	return true;
}

static void handle_surface_destroy(struct wl_listener *listener, void *data) {
	struct wlr_pointer_constraint_v1 *constraint =
		wl_container_of(listener, constraint, surface_destroy);

	pointer_constraint_destroy(constraint);
}

static void handle_seat_destroy(struct wl_listener *listener, void *data) {
	struct wlr_pointer_constraint_v1 *constraint =
		wl_container_of(listener, constraint, seat_destroy);

	pointer_constraint_destroy(constraint);
}

static const struct zwp_confined_pointer_v1_interface confined_pointer_impl = {
	.destroy = resource_destroy,
	.set_region = pointer_constraint_handle_set_region,
};

static const struct zwp_locked_pointer_v1_interface locked_pointer_impl = {
	.destroy = resource_destroy,
	.set_region = pointer_constraint_handle_set_region,
	.set_cursor_position_hint = pointer_constraint_set_cursor_position_hint,
};

static void surface_synced_init_state(void *_state) {
	struct wlr_pointer_constraint_v1_state *state = _state;
	pixman_region32_init(&state->region);
}

static void surface_synced_finish_state(void *_state) {
	struct wlr_pointer_constraint_v1_state *state = _state;
	pixman_region32_fini(&state->region);
}

static void surface_synced_move_state(void *_dst, void *_src) {
	struct wlr_pointer_constraint_v1_state *dst = _dst, *src = _src;

	if (src->committed & WLR_POINTER_CONSTRAINT_V1_STATE_REGION) {
		pixman_region32_copy(&dst->region, &src->region);
	}
	if (src->committed & WLR_POINTER_CONSTRAINT_V1_STATE_CURSOR_HINT) {
		dst->cursor_hint = src->cursor_hint;
	}

	dst->committed = src->committed;
	src->committed = 0;
}

static void surface_synced_commit(struct wlr_surface_synced *synced) {
	struct wlr_pointer_constraint_v1 *constraint = wl_container_of(synced, constraint, synced);

	if (update_region(constraint)) {
		wl_signal_emit_mutable(&constraint->events.set_region, NULL);
	}
}

static const struct wlr_surface_synced_impl surface_synced_impl = {
	.state_size = sizeof(struct wlr_pointer_constraint_v1_state),
	.init_state = surface_synced_init_state,
	.finish_state = surface_synced_finish_state,
	.move_state = surface_synced_move_state,
	.commit = surface_synced_commit,
};

static void pointer_constraint_create(struct wl_client *client,
		struct wl_resource *pointer_constraints_resource, uint32_t id,
		struct wl_resource *surface_resource,
		struct wl_resource *pointer_resource,
		struct wl_resource *region_resource,
		enum zwp_pointer_constraints_v1_lifetime lifetime,
		enum wlr_pointer_constraint_v1_type type) {
	struct wlr_pointer_constraints_v1 *pointer_constraints =
		pointer_constraints_from_resource(pointer_constraints_resource);

	struct wlr_surface *surface = wlr_surface_from_resource(surface_resource);
	struct wlr_seat_client *seat_client =
		wlr_seat_client_from_pointer_resource(pointer_resource);

	uint32_t version = wl_resource_get_version(pointer_constraints_resource);

	bool locked_pointer = type == WLR_POINTER_CONSTRAINT_V1_LOCKED;

	struct wl_resource *resource = locked_pointer ?
		wl_resource_create(client, &zwp_locked_pointer_v1_interface, version, id) :
		wl_resource_create(client, &zwp_confined_pointer_v1_interface, version, id);
	if (resource == NULL) {
		wl_client_post_no_memory(client);
		return;
	}

	void *impl = locked_pointer ?
		(void *)&locked_pointer_impl : (void *)&confined_pointer_impl;
	wl_resource_set_implementation(resource, impl, NULL,
		pointer_constraint_destroy_resource);

	if (seat_client == NULL) {
		// Leave the resource inert
		return;
	}

	struct wlr_seat *seat = seat_client->seat;

	if (wlr_pointer_constraints_v1_constraint_for_surface(pointer_constraints,
			surface, seat)) {
		wl_resource_destroy(resource);
		wl_resource_post_error(pointer_constraints_resource,
			ZWP_POINTER_CONSTRAINTS_V1_ERROR_ALREADY_CONSTRAINED,
			"a pointer constraint with a wl_pointer of the same wl_seat"
			" is already on this surface");
		return;
	}

	struct wlr_pointer_constraint_v1 *constraint = calloc(1, sizeof(*constraint));
	if (constraint == NULL) {
		wl_resource_destroy(resource);
		wl_client_post_no_memory(client);
		return;
	}

	if (!wlr_surface_synced_init(&constraint->synced, surface,
			&surface_synced_impl, &constraint->pending, &constraint->current)) {
		free(constraint);
		wl_resource_destroy(resource);
		wl_client_post_no_memory(client);
		return;
	}

	constraint->resource = resource;
	constraint->surface = surface;
	constraint->seat = seat;
	constraint->lifetime = lifetime;
	constraint->type = type;
	constraint->pointer_constraints = pointer_constraints;

	wl_signal_init(&constraint->events.set_region);
	wl_signal_init(&constraint->events.destroy);

	pixman_region32_init(&constraint->region);

	if (region_resource) {
		pixman_region32_copy(&constraint->current.region,
			wlr_region_from_resource(region_resource));
		update_region(constraint);
	}

	constraint->surface_destroy.notify = handle_surface_destroy;
	wl_signal_add(&surface->events.destroy, &constraint->surface_destroy);

	constraint->seat_destroy.notify = handle_seat_destroy;
	wl_signal_add(&seat->events.destroy, &constraint->seat_destroy);

	wl_resource_set_user_data(resource, constraint);

	wlr_log(WLR_DEBUG, "new %s_pointer %p (res %p)",
		locked_pointer ? "locked" : "confined",
		constraint, constraint->resource);

	wl_list_insert(&pointer_constraints->constraints, &constraint->link);

	wl_signal_emit_mutable(&pointer_constraints->events.new_constraint,
		constraint);
}

static void pointer_constraints_lock_pointer(struct wl_client *client,
		struct wl_resource *cons_resource, uint32_t id,
		struct wl_resource *surface, struct wl_resource *pointer,
		struct wl_resource *region, enum zwp_pointer_constraints_v1_lifetime lifetime) {
	pointer_constraint_create(client, cons_resource, id, surface, pointer,
		region, lifetime, WLR_POINTER_CONSTRAINT_V1_LOCKED);
}

static void pointer_constraints_confine_pointer(struct wl_client *client,
		struct wl_resource *cons_resource, uint32_t id,
		struct wl_resource *surface, struct wl_resource *pointer,
		struct wl_resource *region,
		enum zwp_pointer_constraints_v1_lifetime lifetime) {
	pointer_constraint_create(client, cons_resource, id, surface, pointer,
		region, lifetime, WLR_POINTER_CONSTRAINT_V1_CONFINED);
}

static const struct zwp_pointer_constraints_v1_interface
		pointer_constraints_impl = {
	.destroy = resource_destroy,
	.lock_pointer = pointer_constraints_lock_pointer,
	.confine_pointer = pointer_constraints_confine_pointer,
};

static void pointer_constraints_bind(struct wl_client *client, void *data,
		uint32_t version, uint32_t id) {
	struct wlr_pointer_constraints_v1 *pointer_constraints = data;

	struct wl_resource *resource = wl_resource_create(client,
		&zwp_pointer_constraints_v1_interface, version, id);
	if (resource == NULL) {
		wl_client_post_no_memory(client);
		return;
	}

	wl_resource_set_implementation(resource, &pointer_constraints_impl,
		pointer_constraints, NULL);
}

static void handle_display_destroy(struct wl_listener *listener, void *data) {
	struct wlr_pointer_constraints_v1 *pointer_constraints =
		wl_container_of(listener, pointer_constraints, display_destroy);
	wl_signal_emit_mutable(&pointer_constraints->events.destroy, NULL);

	assert(wl_list_empty(&pointer_constraints->events.destroy.listener_list));
	assert(wl_list_empty(&pointer_constraints->events.new_constraint.listener_list));

	wl_list_remove(&pointer_constraints->display_destroy.link);
	wl_global_destroy(pointer_constraints->global);
	free(pointer_constraints);
}

struct wlr_pointer_constraints_v1 *wlr_pointer_constraints_v1_create(
		struct wl_display *display) {
	struct wlr_pointer_constraints_v1 *pointer_constraints =
		calloc(1, sizeof(*pointer_constraints));
	if (!pointer_constraints) {
		return NULL;
	}

	struct wl_global *wl_global = wl_global_create(display,
		&zwp_pointer_constraints_v1_interface, 1, pointer_constraints,
		pointer_constraints_bind);
	if (!wl_global) {
		free(pointer_constraints);
		return NULL;
	}
	pointer_constraints->global = wl_global;

	wl_list_init(&pointer_constraints->constraints);

	wl_signal_init(&pointer_constraints->events.destroy);
	wl_signal_init(&pointer_constraints->events.new_constraint);

	pointer_constraints->display_destroy.notify = handle_display_destroy;
	wl_display_add_destroy_listener(display,
		&pointer_constraints->display_destroy);

	return pointer_constraints;
}

struct wlr_pointer_constraint_v1 *
		wlr_pointer_constraints_v1_constraint_for_surface(
		struct wlr_pointer_constraints_v1 *pointer_constraints,
		struct wlr_surface *surface, struct wlr_seat *seat) {
	struct wlr_pointer_constraint_v1 *constraint;
	wl_list_for_each(constraint, &pointer_constraints->constraints, link) {
		if (constraint->surface == surface && constraint->seat == seat) {
			return constraint;
		}
	}

	return NULL;
}

void wlr_pointer_constraint_v1_send_activated(
		struct wlr_pointer_constraint_v1 *constraint) {
	wlr_log(WLR_DEBUG, "constrained %p", constraint);
	if (constraint->type == WLR_POINTER_CONSTRAINT_V1_LOCKED) {
		zwp_locked_pointer_v1_send_locked(constraint->resource);
	} else {
		zwp_confined_pointer_v1_send_confined(constraint->resource);
	}
}

void wlr_pointer_constraint_v1_send_deactivated(
		struct wlr_pointer_constraint_v1 *constraint) {
	wlr_log(WLR_DEBUG, "unconstrained %p", constraint);
	if (constraint->type == WLR_POINTER_CONSTRAINT_V1_LOCKED) {
		zwp_locked_pointer_v1_send_unlocked(constraint->resource);
	} else {
		zwp_confined_pointer_v1_send_unconfined(constraint->resource);
	}

	if (constraint->lifetime ==
			ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_ONESHOT) {
		pointer_constraint_destroy(constraint);
	}
}
