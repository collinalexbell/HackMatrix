#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-client-protocol.h>
#include <wayland-egl.h>
#include <wayland-server-core.h>
#include <wlr/backend/wayland.h>
#include <wlr/render/allocator.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/log.h>

#include "xdg-shell-client-protocol.h"

struct surface {
	struct wlr_surface *wlr;
	struct wl_listener commit;
	struct wl_listener destroy;
};

static struct wl_display *remote_display = NULL;
static struct wl_compositor *compositor = NULL;
static struct wl_subcompositor *subcompositor = NULL;
static struct xdg_wm_base *wm_base = NULL;

static struct wl_egl_window *egl_window = NULL;
static struct wlr_egl_surface *egl_surface = NULL;
static struct wl_surface *main_surface = NULL;
static struct wl_callback *frame_callback = NULL;

static struct wlr_scene *scene = NULL;
static struct wlr_scene_output *scene_output = NULL;
static struct wl_listener new_surface = {0};
static struct wl_listener output_frame = {0};

static EGLDisplay egl_display;
static EGLConfig egl_config;
static EGLContext egl_context;

static int width = 500;
static int height = 500;

static void draw_main_surface(void);

static void frame_handle_done(void *data, struct wl_callback *callback, uint32_t t) {
	wl_callback_destroy(callback);
	frame_callback = NULL;
	draw_main_surface();
}

static const struct wl_callback_listener frame_listener = {
	.done = frame_handle_done,
};

static void draw_main_surface(void) {
	frame_callback = wl_surface_frame(main_surface);
	wl_callback_add_listener(frame_callback, &frame_listener, NULL);

	eglMakeCurrent(egl_display, egl_surface, egl_surface, egl_context);
	eglSwapInterval(egl_display, 0);

	glViewport(0, 0, width, height);
	glClearColor(1, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT);

	eglSwapBuffers(egl_display, egl_surface);
	wl_display_flush(remote_display);
}

static void xdg_surface_handle_configure(void *data,
		struct xdg_surface *xdg_surface, uint32_t serial) {
	xdg_surface_ack_configure(xdg_surface, serial);
	if (frame_callback == NULL) {
		draw_main_surface();
	}
}

static const struct xdg_surface_listener xdg_surface_listener = {
	.configure = xdg_surface_handle_configure,
};

static void xdg_toplevel_handle_configure(void *data,
		struct xdg_toplevel *xdg_toplevel, int32_t w, int32_t h,
		struct wl_array *states) {
	if (w != 0 && h != 0) {
		width = w;
		height = h;
	}
}

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
	.configure = xdg_toplevel_handle_configure,
};

static void handle_global(void *data, struct wl_registry *registry,
		uint32_t name, const char *interface, uint32_t version) {
	if (strcmp(interface, wl_compositor_interface.name) == 0) {
		compositor = wl_registry_bind(registry, name, &wl_compositor_interface, 4);
	} else if (strcmp(interface, wl_subcompositor_interface.name) == 0) {
		subcompositor = wl_registry_bind(registry, name, &wl_subcompositor_interface, 1);
	} else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
		wm_base = wl_registry_bind(registry, name, &xdg_wm_base_interface, 1);
	}
}

static void handle_global_remove(void *data, struct wl_registry *registry,
		uint32_t name) {
	// Who cares?
}

static const struct wl_registry_listener registry_listener = {
	.global = handle_global,
	.global_remove = handle_global_remove,
};

static void output_handle_frame(struct wl_listener *listener, void *data) {
	wlr_scene_output_commit(scene_output, NULL);

	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	wlr_scene_output_send_frame_done(scene_output, &now);
}

static void surface_handle_commit(struct wl_listener *listener, void *data) {
	struct surface *surface = wl_container_of(listener, surface, commit);
	struct wlr_xdg_toplevel *xdg_toplevel =
		wlr_xdg_toplevel_try_from_wlr_surface(surface->wlr);
	if (xdg_toplevel != NULL && xdg_toplevel->base->initial_commit) {
		wlr_xdg_toplevel_set_size(xdg_toplevel, 0, 0);
	}
}

static void surface_handle_destroy(struct wl_listener *listener, void *data) {
	struct surface *surface = wl_container_of(listener, surface, destroy);
	wl_list_remove(&surface->commit.link);
	wl_list_remove(&surface->destroy.link);
	free(surface);
}

static void handle_new_surface(struct wl_listener *listener, void *data) {
	struct wlr_surface *wlr_surface = data;

	struct surface *surface = calloc(1, sizeof(*surface));
	surface->wlr = wlr_surface;

	surface->commit.notify = surface_handle_commit;
	wl_signal_add(&wlr_surface->events.commit, &surface->commit);

	surface->destroy.notify = surface_handle_destroy;
	wl_signal_add(&wlr_surface->events.destroy, &surface->destroy);

	wlr_scene_surface_create(&scene->tree, wlr_surface);
}

static void init_egl(struct wl_display *display) {
	egl_display = eglGetPlatformDisplay(EGL_PLATFORM_WAYLAND_KHR, display, NULL);
	eglInitialize(egl_display, NULL, NULL);

	EGLint matched = 0;
	const EGLint config_attribs[] = {
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8,
		EGL_ALPHA_SIZE, 8,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		EGL_NONE,
	};
	eglChooseConfig(egl_display, config_attribs, &egl_config, 1, &matched);

	const EGLint context_attribs[] = {
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE,
	};
	egl_context = eglCreateContext(egl_display, egl_config, EGL_NO_CONTEXT, context_attribs);
}

int main(int argc, char *argv[]) {
	wlr_log_init(WLR_DEBUG, NULL);

	remote_display = wl_display_connect(NULL);
	struct wl_registry *registry = wl_display_get_registry(remote_display);
	wl_registry_add_listener(registry, &registry_listener, NULL);
	wl_display_roundtrip(remote_display);

	init_egl(remote_display);

	struct wl_display *local_display = wl_display_create();
	struct wl_event_loop *event_loop = wl_display_get_event_loop(local_display);
	struct wlr_backend *backend = wlr_wl_backend_create(event_loop, remote_display);
	struct wlr_renderer *renderer = wlr_renderer_autocreate(backend);
	wlr_renderer_init_wl_display(renderer, local_display);
	struct wlr_allocator *allocator = wlr_allocator_autocreate(backend, renderer);
	scene = wlr_scene_create();
	struct wlr_compositor *wlr_compositor = wlr_compositor_create(local_display, 5, renderer);

	wlr_xdg_shell_create(local_display, 2);

	new_surface.notify = handle_new_surface;
	wl_signal_add(&wlr_compositor->events.new_surface, &new_surface);

	wlr_backend_start(backend);

	main_surface = wl_compositor_create_surface(compositor);
	struct xdg_surface *xdg_surface = xdg_wm_base_get_xdg_surface(wm_base, main_surface);
	struct xdg_toplevel *xdg_toplevel = xdg_surface_get_toplevel(xdg_surface);
	xdg_surface_add_listener(xdg_surface, &xdg_surface_listener, NULL);
	xdg_toplevel_add_listener(xdg_toplevel, &xdg_toplevel_listener, NULL);

	egl_window = wl_egl_window_create(main_surface, width, height);
	egl_surface = eglCreatePlatformWindowSurface(egl_display,
		egl_config, egl_window, NULL);

	struct wl_surface *child_surface = wl_compositor_create_surface(compositor);
	struct wl_subsurface *subsurface = wl_subcompositor_get_subsurface(subcompositor, child_surface, main_surface);
	wl_subsurface_set_position(subsurface, 20, 20);
	struct wlr_output *output = wlr_wl_output_create_from_surface(backend, child_surface);
	wlr_output_init_render(output, allocator, renderer);
	scene_output = wlr_scene_output_create(scene, output);

	output_frame.notify = output_handle_frame;
	wl_signal_add(&output->events.frame, &output_frame);

	struct wlr_output_state state;
	wlr_output_state_init(&state);
	wlr_output_state_set_enabled(&state, true);
	wlr_output_commit_state(output, &state);
	wlr_output_state_finish(&state);

	wl_surface_commit(main_surface);
	wl_display_flush(remote_display);

	const char *socket = wl_display_add_socket_auto(local_display);
	setenv("WAYLAND_DISPLAY", socket, true);
	wlr_log(WLR_INFO, "Running embedded Wayland compositor on WAYLAND_DISPLAY=%s", socket);

	wl_display_run(local_display);

	return 0;
}
