#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <wlr/backend.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_presentation_time.h>
#include <wlr/util/addon.h>
#include "presentation-time-protocol.h"

#define PRESENTATION_VERSION 2

struct wlr_presentation_surface_state {
	struct wlr_presentation_feedback *feedback;
};

struct wlr_presentation_surface {
	struct wlr_presentation_surface_state current, pending;

	struct wlr_addon addon; // wlr_surface.addons
	struct wlr_surface_synced synced;
};

static void feedback_handle_resource_destroy(struct wl_resource *resource) {
	wl_list_remove(wl_resource_get_link(resource));
}

static void feedback_resource_send_presented(
		struct wl_resource *feedback_resource,
		const struct wlr_presentation_event *event) {
	struct wl_client *client = wl_resource_get_client(feedback_resource);
	struct wl_resource *output_resource;
	wl_resource_for_each(output_resource, &event->output->resources) {
		if (wl_resource_get_client(output_resource) == client) {
			wp_presentation_feedback_send_sync_output(feedback_resource,
				output_resource);
		}
	}

	uint32_t tv_sec_hi = event->tv_sec >> 32;
	uint32_t tv_sec_lo = event->tv_sec & 0xFFFFFFFF;
	uint32_t seq_hi = event->seq >> 32;
	uint32_t seq_lo = event->seq & 0xFFFFFFFF;
	wp_presentation_feedback_send_presented(feedback_resource,
		tv_sec_hi, tv_sec_lo, event->tv_nsec, event->refresh,
		seq_hi, seq_lo, event->flags);

	wl_resource_destroy(feedback_resource);
}

static void feedback_resource_send_discarded(
		struct wl_resource *feedback_resource) {
	wp_presentation_feedback_send_discarded(feedback_resource);
	wl_resource_destroy(feedback_resource);
}

static const struct wlr_addon_interface presentation_surface_addon_impl;

static void presentation_surface_addon_destroy(struct wlr_addon *addon) {
	struct wlr_presentation_surface *p_surface =
		wl_container_of(addon, p_surface, addon);

	wlr_addon_finish(addon);
	wlr_surface_synced_finish(&p_surface->synced);

	free(p_surface);
}

static const struct wlr_addon_interface presentation_surface_addon_impl = {
	.name = "wlr_presentation_surface",
	.destroy = presentation_surface_addon_destroy,
};

static void surface_synced_finish_state(void *_state) {
	struct wlr_presentation_surface_state *state = _state;
	wlr_presentation_feedback_destroy(state->feedback);
}

static void surface_synced_move_state(void *_dst, void *_src) {
	struct wlr_presentation_surface_state *dst = _dst, *src = _src;
	surface_synced_finish_state(dst);
	dst->feedback = src->feedback;
	src->feedback = NULL;
}

static const struct wlr_surface_synced_impl surface_synced_impl = {
	.state_size = sizeof(struct wlr_presentation_surface_state),
	.finish_state = surface_synced_finish_state,
	.move_state = surface_synced_move_state,
};

static void presentation_handle_feedback(struct wl_client *client,
		struct wl_resource *presentation_resource,
		struct wl_resource *surface_resource, uint32_t id) {
	struct wlr_surface *surface = wlr_surface_from_resource(surface_resource);

	struct wlr_addon *addon =
		wlr_addon_find(&surface->addons, NULL, &presentation_surface_addon_impl);
	struct wlr_presentation_surface *p_surface = NULL;
	if (addon != NULL) {
		p_surface = wl_container_of(addon, p_surface, addon);
	} else {
		p_surface = calloc(1, sizeof(*p_surface));
		if (p_surface == NULL) {
			wl_client_post_no_memory(client);
			return;
		}
		wlr_addon_init(&p_surface->addon, &surface->addons,
			NULL, &presentation_surface_addon_impl);
		if (!wlr_surface_synced_init(&p_surface->synced, surface,
				&surface_synced_impl, &p_surface->pending, &p_surface->current)) {
			free(p_surface);
			wl_client_post_no_memory(client);
			return;
		}
	}

	struct wlr_presentation_feedback *feedback = p_surface->pending.feedback;
	if (feedback == NULL) {
		feedback = calloc(1, sizeof(*feedback));
		if (feedback == NULL) {
			wl_client_post_no_memory(client);
			return;
		}

		wl_list_init(&feedback->resources);
		p_surface->pending.feedback = feedback;
	}

	uint32_t version = wl_resource_get_version(presentation_resource);
	struct wl_resource *resource = wl_resource_create(client,
		&wp_presentation_feedback_interface, version, id);
	if (resource == NULL) {
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(resource, NULL, feedback,
		feedback_handle_resource_destroy);

	wl_list_insert(&feedback->resources, wl_resource_get_link(resource));
}

static void presentation_handle_destroy(struct wl_client *client,
		struct wl_resource *resource) {
	wl_resource_destroy(resource);
}

static const struct wp_presentation_interface presentation_impl = {
	.feedback = presentation_handle_feedback,
	.destroy = presentation_handle_destroy,
};

static void presentation_bind(struct wl_client *client, void *data,
		uint32_t version, uint32_t id) {
	struct wl_resource *resource = wl_resource_create(client,
		&wp_presentation_interface, version, id);
	if (resource == NULL) {
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(resource, &presentation_impl, NULL, NULL);

	wp_presentation_send_clock_id(resource, CLOCK_MONOTONIC);
}

static void handle_display_destroy(struct wl_listener *listener, void *data) {
	struct wlr_presentation *presentation =
		wl_container_of(listener, presentation, display_destroy);
	wl_signal_emit_mutable(&presentation->events.destroy, presentation);

	assert(wl_list_empty(&presentation->events.destroy.listener_list));

	wl_list_remove(&presentation->display_destroy.link);
	wl_global_destroy(presentation->global);
	free(presentation);
}

struct wlr_presentation *wlr_presentation_create(struct wl_display *display,
		struct wlr_backend *backend, uint32_t version) {
	assert(version <= PRESENTATION_VERSION);

	struct wlr_presentation *presentation = calloc(1, sizeof(*presentation));
	if (presentation == NULL) {
		return NULL;
	}

	presentation->global = wl_global_create(display, &wp_presentation_interface,
		version, NULL, presentation_bind);
	if (presentation->global == NULL) {
		free(presentation);
		return NULL;
	}

	wl_signal_init(&presentation->events.destroy);

	presentation->display_destroy.notify = handle_display_destroy;
	wl_display_add_destroy_listener(display, &presentation->display_destroy);

	return presentation;
}

void wlr_presentation_feedback_send_presented(
		struct wlr_presentation_feedback *feedback,
		const struct wlr_presentation_event *event) {
	struct wl_resource *resource, *tmp;
	wl_resource_for_each_safe(resource, tmp, &feedback->resources) {
		feedback_resource_send_presented(resource, event);
	}
}

struct wlr_presentation_feedback *wlr_presentation_surface_sampled(
		struct wlr_surface *surface) {
	struct wlr_addon *addon =
		wlr_addon_find(&surface->addons, NULL, &presentation_surface_addon_impl);
	if (addon != NULL) {
		struct wlr_presentation_surface *p_surface =
			wl_container_of(addon, p_surface, addon);
		struct wlr_presentation_feedback *sampled =
			p_surface->current.feedback;
		p_surface->current.feedback = NULL;
		return sampled;
	}
	return NULL;
}

static void feedback_unset_output(struct wlr_presentation_feedback *feedback);

void wlr_presentation_feedback_destroy(
		struct wlr_presentation_feedback *feedback) {
	if (feedback == NULL) {
		return;
	}

	struct wl_resource *resource, *tmp;
	wl_resource_for_each_safe(resource, tmp, &feedback->resources) {
		feedback_resource_send_discarded(resource);
	}
	assert(wl_list_empty(&feedback->resources));

	feedback_unset_output(feedback);
	free(feedback);
}

void wlr_presentation_event_from_output(struct wlr_presentation_event *event,
		const struct wlr_output_event_present *output_event) {
	*event = (struct wlr_presentation_event){
		.output = output_event->output,
		.tv_sec = (uint64_t)output_event->when.tv_sec,
		.tv_nsec = (uint32_t)output_event->when.tv_nsec,
		.refresh = (uint32_t)output_event->refresh,
		.seq = (uint64_t)output_event->seq,
		.flags = output_event->flags,
	};
}

static void feedback_unset_output(struct wlr_presentation_feedback *feedback) {
	if (feedback->output == NULL) {
		return;
	}

	feedback->output = NULL;
	wl_list_remove(&feedback->output_commit.link);
	wl_list_remove(&feedback->output_present.link);
	wl_list_remove(&feedback->output_destroy.link);
}

static void feedback_handle_output_commit(struct wl_listener *listener,
		void *data) {
	struct wlr_presentation_feedback *feedback =
		wl_container_of(listener, feedback, output_commit);
	if (feedback->output_committed) {
		return;
	}
	feedback->output_committed = true;
	feedback->output_commit_seq = feedback->output->commit_seq;
}

static void feedback_handle_output_present(struct wl_listener *listener,
		void *data) {
	struct wlr_presentation_feedback *feedback =
		wl_container_of(listener, feedback, output_present);
	struct wlr_output_event_present *output_event = data;

	if (!feedback->output_committed ||
			output_event->commit_seq != feedback->output_commit_seq) {
		return;
	}

	if (output_event->presented) {
		struct wlr_presentation_event event = {0};
		wlr_presentation_event_from_output(&event, output_event);
		struct wl_resource *resource = wl_resource_from_link(feedback->resources.next);
		if (wl_resource_get_version(resource) == 1 &&
				event.output->adaptive_sync_status == WLR_OUTPUT_ADAPTIVE_SYNC_ENABLED) {
			event.refresh = 0;
		}
		if (!feedback->zero_copy) {
			event.flags &= ~WP_PRESENTATION_FEEDBACK_KIND_ZERO_COPY;
		}
		wlr_presentation_feedback_send_presented(feedback, &event);
	}
	wlr_presentation_feedback_destroy(feedback);
}

static void feedback_handle_output_destroy(struct wl_listener *listener,
		void *data) {
	struct wlr_presentation_feedback *feedback =
		wl_container_of(listener, feedback, output_destroy);
	wlr_presentation_feedback_destroy(feedback);
}

static void presentation_surface_queued_on_output(struct wlr_surface *surface,
		struct wlr_output *output, bool zero_copy) {
	struct wlr_presentation_feedback *feedback =
		wlr_presentation_surface_sampled(surface);
	if (feedback == NULL) {
		return;
	}

	assert(feedback->output == NULL);
	feedback->output = output;
	feedback->zero_copy = zero_copy;

	feedback->output_commit.notify = feedback_handle_output_commit;
	wl_signal_add(&output->events.commit, &feedback->output_commit);
	feedback->output_present.notify = feedback_handle_output_present;
	wl_signal_add(&output->events.present, &feedback->output_present);
	feedback->output_destroy.notify = feedback_handle_output_destroy;
	wl_signal_add(&output->events.destroy, &feedback->output_destroy);
}

void wlr_presentation_surface_textured_on_output(struct wlr_surface *surface,
		struct wlr_output *output) {
	return presentation_surface_queued_on_output(surface, output, false);
}

void wlr_presentation_surface_scanned_out_on_output(struct wlr_surface *surface,
		struct wlr_output *output) {
	return presentation_surface_queued_on_output(surface, output, true);
}
