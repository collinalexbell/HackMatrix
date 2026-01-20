
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#include <drm_fourcc.h>
#include <xcb/dri3.h>
#include <xcb/present.h>
#include <xcb/render.h>
#include <xcb/shm.h>
#include <xcb/xcb.h>
#include <xcb/xinput.h>

#include <wlr/interfaces/wlr_output.h>
#include <wlr/interfaces/wlr_pointer.h>
#include <wlr/interfaces/wlr_touch.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/util/log.h>

#include "backend/x11.h"
#include "util/time.h"
#include "types/wlr_output.h"

static const uint32_t SUPPORTED_OUTPUT_STATE =
	WLR_OUTPUT_STATE_BACKEND_OPTIONAL |
	WLR_OUTPUT_STATE_BUFFER |
	WLR_OUTPUT_STATE_ENABLED |
	WLR_OUTPUT_STATE_MODE |
	WLR_OUTPUT_STATE_ADAPTIVE_SYNC_ENABLED;

static size_t last_output_num = 0;

static void parse_xcb_setup(struct wlr_output *output,
		xcb_connection_t *xcb) {
	const xcb_setup_t *xcb_setup = xcb_get_setup(xcb);

	output->make = calloc(1, xcb_setup_vendor_length(xcb_setup) + 1);
	if (output->make == NULL) {
		wlr_log_errno(WLR_ERROR, "Allocation failed");
		return;
	}
	memcpy(output->make, xcb_setup_vendor(xcb_setup),
		xcb_setup_vendor_length(xcb_setup));

	char model[64];
	snprintf(model, sizeof(model), "%"PRIu16".%"PRIu16,
		xcb_setup->protocol_major_version,
		xcb_setup->protocol_minor_version);
	output->model = strdup(model);
}

static struct wlr_x11_output *get_x11_output_from_output(
		struct wlr_output *wlr_output) {
	assert(wlr_output_is_x11(wlr_output));
	struct wlr_x11_output *output = wl_container_of(wlr_output, output, wlr_output);
	return output;
}

static bool output_set_custom_mode(struct wlr_output *wlr_output,
		int32_t width, int32_t height, int32_t refresh) {
	struct wlr_x11_output *output = get_x11_output_from_output(wlr_output);
	struct wlr_x11_backend *x11 = output->x11;

	if (width == output->win_width && height == output->win_height) {
		return true;
	}

	const uint32_t values[] = { width, height };
	xcb_void_cookie_t cookie = xcb_configure_window_checked(
		x11->xcb, output->win,
		XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, values);

	xcb_generic_error_t *error;
	if ((error = xcb_request_check(x11->xcb, cookie))) {
		wlr_log(WLR_ERROR, "Could not set window size to %dx%d\n",
			width, height);
		free(error);
		return false;
	}

	output->win_width = width;
	output->win_height = height;

	// Move the pointer to its new location
	update_x11_pointer_position(output, output->x11->time);

	return true;
}

static void destroy_x11_buffer(struct wlr_x11_buffer *buffer);

static void output_destroy(struct wlr_output *wlr_output) {
	struct wlr_x11_output *output = get_x11_output_from_output(wlr_output);
	struct wlr_x11_backend *x11 = output->x11;

	wlr_output_finish(wlr_output);

	pixman_region32_fini(&output->exposed);

	wlr_pointer_finish(&output->pointer);
	wlr_touch_finish(&output->touch);

	struct wlr_x11_buffer *buffer, *buffer_tmp;
	wl_list_for_each_safe(buffer, buffer_tmp, &output->buffers, link) {
		destroy_x11_buffer(buffer);
	}

	wl_list_remove(&output->link);

	if (output->cursor.pic != XCB_NONE) {
		xcb_render_free_picture(x11->xcb, output->cursor.pic);
	}

	// A zero event mask deletes the event context
	xcb_present_select_input(x11->xcb, output->present_event_id, output->win, 0);
	xcb_destroy_window(x11->xcb, output->win);
	xcb_flush(x11->xcb);
	free(output);
}

static bool output_test(struct wlr_output *wlr_output,
		const struct wlr_output_state *state) {
	struct wlr_x11_output *output = get_x11_output_from_output(wlr_output);
	struct wlr_x11_backend *x11 = output->x11;

	uint32_t unsupported = state->committed & ~SUPPORTED_OUTPUT_STATE;
	if (unsupported != 0) {
		wlr_log(WLR_DEBUG, "Unsupported output state fields: 0x%"PRIx32,
			unsupported);
		return false;
	}

	if (state->committed & WLR_OUTPUT_STATE_BUFFER) {
		// If the size doesn't match, reject buffer (scaling is not supported)
		int pending_width, pending_height;
		output_pending_resolution(wlr_output, state,
			&pending_width, &pending_height);
		if (state->buffer->width != pending_width ||
				state->buffer->height != pending_height) {
			wlr_log(WLR_DEBUG, "Primary buffer size mismatch");
			return false;
		}
		// Source crop is not supported
		struct wlr_fbox src_box;
		output_state_get_buffer_src_box(state, &src_box);
		if (src_box.x != 0.0 || src_box.y != 0.0 ||
				src_box.width != (double)state->buffer->width ||
				src_box.height != (double)state->buffer->height) {
			wlr_log(WLR_DEBUG, "Source crop not supported in X11 output");
			return false;
		}
	}

	// All we can do to influence adaptive sync on the X11 backend is set the
	// _VARIABLE_REFRESH window property like mesa automatically does. We don't
	// have any control beyond that, so we set the state to enabled on creating
	// the output and never allow changing it (just like the Wayland backend).
	assert(wlr_output->adaptive_sync_status == WLR_OUTPUT_ADAPTIVE_SYNC_ENABLED);
	if (state->committed & WLR_OUTPUT_STATE_ADAPTIVE_SYNC_ENABLED) {
		if (!state->adaptive_sync_enabled) {
			wlr_log(WLR_DEBUG, "Disabling adaptive sync is not supported");
			return false;
		}
	}

	if (state->committed & WLR_OUTPUT_STATE_BUFFER) {
		struct wlr_buffer *buffer = state->buffer;
		struct wlr_dmabuf_attributes dmabuf_attrs;
		struct wlr_shm_attributes shm_attrs;
		uint32_t format = DRM_FORMAT_INVALID;
		if (wlr_buffer_get_dmabuf(buffer, &dmabuf_attrs)) {
			format = dmabuf_attrs.format;
		} else if (wlr_buffer_get_shm(buffer, &shm_attrs)) {
			format = shm_attrs.format;
		}
		if (format != x11->x11_format->drm) {
			wlr_log(WLR_DEBUG, "Unsupported buffer format");
			return false;
		}
	}

	if (state->committed & WLR_OUTPUT_STATE_MODE) {
		assert(state->mode_type == WLR_OUTPUT_STATE_MODE_CUSTOM);

		if (state->custom_mode.refresh != 0) {
			wlr_log(WLR_DEBUG, "Refresh rates are not supported");
			return false;
		}
	}

	return true;
}

static void destroy_x11_buffer(struct wlr_x11_buffer *buffer) {
	if (!buffer) {
		return;
	}
	wl_list_remove(&buffer->buffer_destroy.link);
	wl_list_remove(&buffer->link);
	xcb_free_pixmap(buffer->x11->xcb, buffer->pixmap);
	for (size_t i = 0; i < buffer->n_busy; i++) {
		wlr_buffer_unlock(buffer->buffer);
	}
	free(buffer);
}

static void buffer_handle_buffer_destroy(struct wl_listener *listener,
		void *data) {
	struct wlr_x11_buffer *buffer =
		wl_container_of(listener, buffer, buffer_destroy);
	destroy_x11_buffer(buffer);
}

static xcb_pixmap_t import_dmabuf(struct wlr_x11_output *output,
		struct wlr_dmabuf_attributes *dmabuf) {
	struct wlr_x11_backend *x11 = output->x11;

	if (dmabuf->format != x11->x11_format->drm) {
		// The pixmap's depth must match the window's depth, otherwise Present
		// will throw a Match error
		return XCB_PIXMAP_NONE;
	}

	// xcb closes the FDs after sending them, so we need to dup them here
	struct wlr_dmabuf_attributes dup_attrs = {0};
	if (!wlr_dmabuf_attributes_copy(&dup_attrs, dmabuf)) {
		return XCB_PIXMAP_NONE;
	}

	const struct wlr_x11_format *x11_fmt = x11->x11_format;
	xcb_pixmap_t pixmap = xcb_generate_id(x11->xcb);

	if (x11->dri3_major_version > 1 || x11->dri3_minor_version >= 2) {
		if (dmabuf->n_planes > 4) {
			wlr_dmabuf_attributes_finish(&dup_attrs);
			return XCB_PIXMAP_NONE;
		}
		xcb_dri3_pixmap_from_buffers(x11->xcb, pixmap, output->win,
			dmabuf->n_planes, dmabuf->width, dmabuf->height, dmabuf->stride[0],
			dmabuf->offset[0], dmabuf->stride[1], dmabuf->offset[1],
			dmabuf->stride[2], dmabuf->offset[2], dmabuf->stride[3],
			dmabuf->offset[3], x11_fmt->depth, x11_fmt->bpp, dmabuf->modifier,
			dup_attrs.fd);
	} else {
		// PixmapFromBuffers requires DRI3 1.2
		if (dmabuf->n_planes != 1
				|| dmabuf->modifier != DRM_FORMAT_MOD_INVALID) {
			wlr_dmabuf_attributes_finish(&dup_attrs);
			return XCB_PIXMAP_NONE;
		}
		xcb_dri3_pixmap_from_buffer(x11->xcb, pixmap, output->win,
			dmabuf->height * dmabuf->stride[0], dmabuf->width, dmabuf->height,
			dmabuf->stride[0], x11_fmt->depth, x11_fmt->bpp, dup_attrs.fd[0]);
	}

	return pixmap;
}

static xcb_pixmap_t import_shm(struct wlr_x11_output *output,
		struct wlr_shm_attributes *shm) {
	struct wlr_x11_backend *x11 = output->x11;

	if (shm->format != x11->x11_format->drm) {
		// The pixmap's depth must match the window's depth, otherwise Present
		// will throw a Match error
		return XCB_PIXMAP_NONE;
	}

	// xcb closes the FD after sending it
	int fd = fcntl(shm->fd, F_DUPFD_CLOEXEC, 0);
	if (fd < 0) {
		wlr_log_errno(WLR_ERROR, "fcntl(F_DUPFD_CLOEXEC) failed");
		return XCB_PIXMAP_NONE;
	}

	xcb_shm_seg_t seg = xcb_generate_id(x11->xcb);
	xcb_shm_attach_fd(x11->xcb, seg, fd, false);

	xcb_pixmap_t pixmap = xcb_generate_id(x11->xcb);
	xcb_shm_create_pixmap(x11->xcb, pixmap, output->win, shm->width,
		shm->height, x11->x11_format->depth, seg, shm->offset);

	xcb_shm_detach(x11->xcb, seg);

	return pixmap;
}

static struct wlr_x11_buffer *create_x11_buffer(struct wlr_x11_output *output,
		struct wlr_buffer *wlr_buffer) {
	struct wlr_x11_backend *x11 = output->x11;
	xcb_pixmap_t pixmap = XCB_PIXMAP_NONE;

	struct wlr_dmabuf_attributes dmabuf_attrs;
	struct wlr_shm_attributes shm_attrs;
	if (wlr_buffer_get_dmabuf(wlr_buffer, &dmabuf_attrs)) {
		pixmap = import_dmabuf(output, &dmabuf_attrs);
	} else if (wlr_buffer_get_shm(wlr_buffer, &shm_attrs)) {
		pixmap = import_shm(output, &shm_attrs);
	}

	if (pixmap == XCB_PIXMAP_NONE) {
		return NULL;
	}

	struct wlr_x11_buffer *buffer = calloc(1, sizeof(*buffer));
	if (!buffer) {
		xcb_free_pixmap(x11->xcb, pixmap);
		return NULL;
	}
	buffer->buffer = wlr_buffer_lock(wlr_buffer);
	buffer->n_busy = 1;
	buffer->pixmap = pixmap;
	buffer->x11 = x11;
	wl_list_insert(&output->buffers, &buffer->link);

	buffer->buffer_destroy.notify = buffer_handle_buffer_destroy;
	wl_signal_add(&wlr_buffer->events.destroy, &buffer->buffer_destroy);

	return buffer;
}

static struct wlr_x11_buffer *get_or_create_x11_buffer(
		struct wlr_x11_output *output, struct wlr_buffer *wlr_buffer) {
	struct wlr_x11_buffer *buffer;
	wl_list_for_each(buffer, &output->buffers, link) {
		if (buffer->buffer == wlr_buffer) {
			wlr_buffer_lock(buffer->buffer);
			buffer->n_busy++;
			return buffer;
		}
	}

	return create_x11_buffer(output, wlr_buffer);
}

static bool output_commit_buffer(struct wlr_x11_output *output,
		const struct wlr_output_state *state) {
	struct wlr_x11_backend *x11 = output->x11;

	struct wlr_buffer *buffer = state->buffer;
	struct wlr_x11_buffer *x11_buffer =
		get_or_create_x11_buffer(output, buffer);
	if (!x11_buffer) {
		goto error;
	}

	xcb_xfixes_region_t region = XCB_NONE;
	if (state->committed & WLR_OUTPUT_STATE_DAMAGE) {
		pixman_region32_union(&output->exposed, &output->exposed, &state->damage);

		int rects_len = 0;
		const pixman_box32_t *rects = pixman_region32_rectangles(&output->exposed, &rects_len);

		xcb_rectangle_t *xcb_rects = calloc(rects_len, sizeof(xcb_rectangle_t));
		if (!xcb_rects) {
			goto error;
		}

		for (int i = 0; i < rects_len; i++) {
			const pixman_box32_t *box = &rects[i];
			xcb_rects[i] = (struct xcb_rectangle_t){
				.x = box->x1,
				.y = box->y1,
				.width = box->x2 - box->x1,
				.height = box->y2 - box->y1,
			};
		}

		region = xcb_generate_id(x11->xcb);
		xcb_xfixes_create_region(x11->xcb, region, rects_len, xcb_rects);

		free(xcb_rects);
	}

	pixman_region32_clear(&output->exposed);

	uint32_t serial = output->wlr_output.commit_seq;
	uint32_t options = 0;
	uint64_t target_msc = output->last_msc ? output->last_msc + 1 : 0;
	xcb_present_pixmap(x11->xcb, output->win, x11_buffer->pixmap, serial,
		0, region, 0, 0, XCB_NONE, XCB_NONE, XCB_NONE, options, target_msc,
		0, 0, 0, NULL);

	if (region != XCB_NONE) {
		xcb_xfixes_destroy_region(x11->xcb, region);
	}

	return true;

error:
	destroy_x11_buffer(x11_buffer);
	return false;
}

static bool output_commit(struct wlr_output *wlr_output,
		const struct wlr_output_state *state) {
	struct wlr_x11_output *output = get_x11_output_from_output(wlr_output);
	struct wlr_x11_backend *x11 = output->x11;

	if (!output_test(wlr_output, state)) {
		return false;
	}

	if (state->committed & WLR_OUTPUT_STATE_ENABLED) {
		if (state->enabled) {
			xcb_map_window(x11->xcb, output->win);
		} else {
			xcb_unmap_window(x11->xcb, output->win);
		}
	}

	if (state->committed & WLR_OUTPUT_STATE_MODE) {
		if (!output_set_custom_mode(wlr_output,
				state->custom_mode.width,
				state->custom_mode.height,
				state->custom_mode.refresh)) {
			return false;
		}
	}

	if (state->committed & WLR_OUTPUT_STATE_BUFFER) {
		if (!output_commit_buffer(output, state)) {
			return false;
		}
	} else if (output_pending_enabled(wlr_output, state)) {
		uint32_t serial = output->wlr_output.commit_seq;
		uint64_t target_msc = output->last_msc ? output->last_msc + 1 : 0;
		xcb_present_notify_msc(x11->xcb, output->win, serial, target_msc, 0, 0);
	}

	xcb_flush(x11->xcb);

	return true;
}

static void update_x11_output_cursor(struct wlr_x11_output *output,
		int32_t hotspot_x, int32_t hotspot_y) {
	struct wlr_x11_backend *x11 = output->x11;

	xcb_cursor_t cursor = x11->transparent_cursor;

	if (output->cursor.pic != XCB_NONE) {
		cursor = xcb_generate_id(x11->xcb);
		xcb_render_create_cursor(x11->xcb, cursor, output->cursor.pic,
			hotspot_x, hotspot_y);
	}

	uint32_t values[] = {cursor};
	xcb_change_window_attributes(x11->xcb, output->win,
		XCB_CW_CURSOR, values);
	xcb_flush(x11->xcb);

	if (cursor != x11->transparent_cursor) {
		xcb_free_cursor(x11->xcb, cursor);
	}
}

static bool output_cursor_to_picture(struct wlr_x11_output *output,
		struct wlr_buffer *buffer) {
	struct wlr_x11_backend *x11 = output->x11;
	struct wlr_renderer *renderer = output->wlr_output.renderer;

	if (output->cursor.pic != XCB_NONE) {
		xcb_render_free_picture(x11->xcb, output->cursor.pic);
	}
	output->cursor.pic = XCB_NONE;

	if (buffer == NULL) {
		return true;
	}

	struct wlr_texture *texture = wlr_texture_from_buffer(renderer, buffer);
	if (!texture) {
		return false;
	}

	int depth = 32;
	int stride = texture->width * 4;
	uint8_t *data = malloc(texture->height * stride);
	if (data == NULL) {
		wlr_texture_destroy(texture);
		return false;
	}

	bool result = wlr_texture_read_pixels(texture, &(struct wlr_texture_read_pixels_options) {
		.format = DRM_FORMAT_ARGB8888,
		.stride = stride,
		.data = data,
	});

	wlr_texture_destroy(texture);

	if (!result) {
		free(data);
		return false;
	}

	xcb_pixmap_t pix = xcb_generate_id(x11->xcb);
	xcb_create_pixmap(x11->xcb, depth, pix, output->win,
		buffer->width, buffer->height);

	output->cursor.pic = xcb_generate_id(x11->xcb);
	xcb_render_create_picture(x11->xcb, output->cursor.pic,
		pix, x11->argb32, 0, 0);

	xcb_gcontext_t gc = xcb_generate_id(x11->xcb);
	xcb_create_gc(x11->xcb, gc, pix, 0, NULL);

	xcb_put_image(x11->xcb, XCB_IMAGE_FORMAT_Z_PIXMAP,
		pix, gc, buffer->width, buffer->height, 0, 0, 0, depth,
		stride * buffer->height * sizeof(uint8_t), data);
	free(data);
	xcb_free_gc(x11->xcb, gc);
	xcb_free_pixmap(x11->xcb, pix);

	return true;
}

static bool output_set_cursor(struct wlr_output *wlr_output,
		struct wlr_buffer *buffer, int32_t hotspot_x, int32_t hotspot_y) {
	struct wlr_x11_output *output = get_x11_output_from_output(wlr_output);
	struct wlr_x11_backend *x11 = output->x11;

	if (x11->argb32 == XCB_NONE) {
		return false;
	}

	if (buffer != NULL) {
		if (hotspot_x < 0) {
			hotspot_x = 0;
		}
		if (hotspot_x > buffer->width) {
			hotspot_x = buffer->width;
		}
		if (hotspot_y < 0) {
			hotspot_y = 0;
		}
		if (hotspot_y > buffer->height) {
			hotspot_y = buffer->height;
		}
	}

	bool success = output_cursor_to_picture(output, buffer);

	update_x11_output_cursor(output, hotspot_x, hotspot_y);

	return success;
}

static bool output_move_cursor(struct wlr_output *_output, int x, int y) {
	// TODO: only return true if x == current x and y == current y
	return true;
}

static const struct wlr_drm_format_set *output_get_primary_formats(
		struct wlr_output *wlr_output, uint32_t buffer_caps) {
	struct wlr_x11_output *output = get_x11_output_from_output(wlr_output);
	struct wlr_x11_backend *x11 = output->x11;

	if (x11->have_dri3 && (buffer_caps & WLR_BUFFER_CAP_DMABUF)) {
		return &output->x11->primary_dri3_formats;
	} else if (x11->have_shm && (buffer_caps & WLR_BUFFER_CAP_SHM)) {
		return &output->x11->primary_shm_formats;
	}
	return NULL;
}

static const struct wlr_output_impl output_impl = {
	.destroy = output_destroy,
	.test = output_test,
	.commit = output_commit,
	.set_cursor = output_set_cursor,
	.move_cursor = output_move_cursor,
	.get_primary_formats = output_get_primary_formats,
};

struct wlr_output *wlr_x11_output_create(struct wlr_backend *backend) {
	struct wlr_x11_backend *x11 = get_x11_backend_from_backend(backend);

	if (!x11->started) {
		++x11->requested_outputs;
		return NULL;
	}

	struct wlr_x11_output *output = calloc(1, sizeof(*output));
	if (output == NULL) {
		return NULL;
	}
	output->x11 = x11;
	wl_list_init(&output->buffers);
	pixman_region32_init(&output->exposed);

	struct wlr_output *wlr_output = &output->wlr_output;

	struct wlr_output_state state;
	wlr_output_state_init(&state);
	wlr_output_state_set_custom_mode(&state, 1024, 768, 0);

	wlr_output_init(wlr_output, &x11->backend, &output_impl, x11->event_loop, &state);
	wlr_output_state_finish(&state);

	size_t output_num = ++last_output_num;

	char name[64];
	snprintf(name, sizeof(name), "X11-%zu", output_num);
	wlr_output_set_name(wlr_output, name);

	parse_xcb_setup(wlr_output, x11->xcb);

	char description[128];
	snprintf(description, sizeof(description), "X11 output %zu", output_num);
	wlr_output_set_description(wlr_output, description);

	// The X11 protocol requires us to set a colormap and border pixel if the
	// depth doesn't match the root window's
	uint32_t mask = XCB_CW_BORDER_PIXEL | XCB_CW_EVENT_MASK |
		XCB_CW_COLORMAP | XCB_CW_CURSOR;
	uint32_t values[] = {
		0,
		XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_STRUCTURE_NOTIFY,
		x11->colormap,
		x11->transparent_cursor,
	};
	output->win = xcb_generate_id(x11->xcb);
	xcb_create_window(x11->xcb, x11->depth->depth, output->win,
		x11->screen->root, 0, 0, wlr_output->width, wlr_output->height, 0,
		XCB_WINDOW_CLASS_INPUT_OUTPUT, x11->visualid, mask, values);

	output->win_width = wlr_output->width;
	output->win_height = wlr_output->height;

	struct {
		xcb_input_event_mask_t head;
		xcb_input_xi_event_mask_t mask;
	} xinput_mask = {
		.head = { .deviceid = XCB_INPUT_DEVICE_ALL_MASTER, .mask_len = 1 },
		.mask = XCB_INPUT_XI_EVENT_MASK_KEY_PRESS |
			XCB_INPUT_XI_EVENT_MASK_KEY_RELEASE |
			XCB_INPUT_XI_EVENT_MASK_BUTTON_PRESS |
			XCB_INPUT_XI_EVENT_MASK_BUTTON_RELEASE |
			XCB_INPUT_XI_EVENT_MASK_MOTION |
			XCB_INPUT_XI_EVENT_MASK_TOUCH_BEGIN |
			XCB_INPUT_XI_EVENT_MASK_TOUCH_END |
			XCB_INPUT_XI_EVENT_MASK_TOUCH_UPDATE,
	};
	xcb_input_xi_select_events(x11->xcb, output->win, 1, &xinput_mask.head);

	uint32_t present_mask = XCB_PRESENT_EVENT_MASK_IDLE_NOTIFY |
		XCB_PRESENT_EVENT_MASK_COMPLETE_NOTIFY;
	output->present_event_id = xcb_generate_id(x11->xcb);
	xcb_present_select_input(x11->xcb, output->present_event_id, output->win,
		present_mask);

	xcb_change_property(x11->xcb, XCB_PROP_MODE_REPLACE, output->win,
		x11->atoms.wm_protocols, XCB_ATOM_ATOM, 32, 1,
		&x11->atoms.wm_delete_window);

	uint32_t enabled = 1;
	xcb_change_property(x11->xcb, XCB_PROP_MODE_REPLACE, output->win,
		x11->atoms.variable_refresh, XCB_ATOM_CARDINAL, 32, 1,
		&enabled);
	wlr_output->adaptive_sync_status = WLR_OUTPUT_ADAPTIVE_SYNC_ENABLED;

	wlr_x11_output_set_title(wlr_output, NULL);

	xcb_flush(x11->xcb);

	wl_list_insert(&x11->outputs, &output->link);

	wlr_pointer_init(&output->pointer, &x11_pointer_impl, "x11-pointer");
	output->pointer.output_name = strdup(wlr_output->name);

	wlr_touch_init(&output->touch, &x11_touch_impl, "x11-touch");
	output->touch.output_name = strdup(wlr_output->name);
	wl_list_init(&output->touchpoints);

	wl_signal_emit_mutable(&x11->backend.events.new_output, wlr_output);
	wl_signal_emit_mutable(&x11->backend.events.new_input, &output->pointer.base);
	wl_signal_emit_mutable(&x11->backend.events.new_input, &output->touch.base);

	return wlr_output;
}

void handle_x11_configure_notify(struct wlr_x11_output *output,
		xcb_configure_notify_event_t *ev) {
	if (ev->width == 0 || ev->height == 0) {
		wlr_log(WLR_DEBUG,
			"Ignoring X11 configure event for height=%d, width=%d",
			ev->width, ev->height);
		return;
	}

	output->win_width = ev->width;
	output->win_height = ev->height;

	struct wlr_output_state state;
	wlr_output_state_init(&state);
	wlr_output_state_set_custom_mode(&state, ev->width, ev->height, 0);
	wlr_output_send_request_state(&output->wlr_output, &state);
	wlr_output_state_finish(&state);
}

bool wlr_output_is_x11(struct wlr_output *wlr_output) {
	return wlr_output->impl == &output_impl;
}

void wlr_x11_output_set_title(struct wlr_output *output, const char *title) {
	struct wlr_x11_output *x11_output = get_x11_output_from_output(output);

	char wl_title[32];
	if (title == NULL) {
		if (snprintf(wl_title, sizeof(wl_title), "wlroots - %s", output->name) <= 0) {
			return;
		}
		title = wl_title;
	}

	xcb_change_property(x11_output->x11->xcb, XCB_PROP_MODE_REPLACE, x11_output->win,
		x11_output->x11->atoms.net_wm_name, x11_output->x11->atoms.utf8_string, 8,
		strlen(title), title);
}

static struct wlr_x11_buffer *get_x11_buffer(struct wlr_x11_output *output,
		xcb_pixmap_t pixmap) {
	struct wlr_x11_buffer *buffer;
	wl_list_for_each(buffer, &output->buffers, link) {
		if (buffer->pixmap == pixmap) {
			return buffer;
		}
	}
	return NULL;
}

void handle_x11_present_event(struct wlr_x11_backend *x11,
		xcb_ge_generic_event_t *event) {
	struct wlr_x11_output *output;

	switch (event->event_type) {
	case XCB_PRESENT_EVENT_IDLE_NOTIFY:;
		xcb_present_idle_notify_event_t *idle_notify =
			(xcb_present_idle_notify_event_t *)event;

		output = get_x11_output_from_window_id(x11, idle_notify->window);
		if (!output) {
			wlr_log(WLR_DEBUG, "Got PresentIdleNotify event for unknown window");
			return;
		}

		struct wlr_x11_buffer *buffer =
			get_x11_buffer(output, idle_notify->pixmap);
		if (!buffer) {
			wlr_log(WLR_DEBUG, "Got PresentIdleNotify event for unknown buffer");
			return;
		}

		assert(buffer->n_busy > 0);
		buffer->n_busy--;
		wlr_buffer_unlock(buffer->buffer); // may destroy buffer
		break;
	case XCB_PRESENT_COMPLETE_NOTIFY:;
		xcb_present_complete_notify_event_t *complete_notify =
			(xcb_present_complete_notify_event_t *)event;

		output = get_x11_output_from_window_id(x11, complete_notify->window);
		if (!output) {
			wlr_log(WLR_DEBUG, "Got PresentCompleteNotify event for unknown window");
			return;
		}

		output->last_msc = complete_notify->msc;

		uint32_t flags = 0;
		if (complete_notify->mode == XCB_PRESENT_COMPLETE_MODE_FLIP) {
			flags |= WLR_OUTPUT_PRESENT_ZERO_COPY;
		}

		bool presented = complete_notify->mode != XCB_PRESENT_COMPLETE_MODE_SKIP;
		struct wlr_output_event_present present_event = {
			.output = &output->wlr_output,
			.commit_seq = complete_notify->serial,
			.presented = presented,
			.seq = complete_notify->msc,
			.flags = flags,
		};
		timespec_from_nsec(&present_event.when, complete_notify->ust * 1000);
		wlr_output_send_present(&output->wlr_output, &present_event);

		wlr_output_send_frame(&output->wlr_output);
		break;
	default:
		wlr_log(WLR_DEBUG, "Unhandled Present event %"PRIu16, event->event_type);
	}
}
