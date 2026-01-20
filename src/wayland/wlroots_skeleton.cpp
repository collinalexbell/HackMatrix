#define WLR_USE_UNSTABLE 1

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <ctime>

extern "C" {
#include <wayland-server-core.h>

#include <wlr/backend.h>
#include <wlr/render/wlr_allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/log.h>
}

namespace {

const float kBackgroundColor[4] = {0.04f, 0.04f, 0.06f, 1.0f};

struct HackMatrixWaylandServer;

struct HackMatrixOutput {
    HackMatrixWaylandServer *server = nullptr;
    wlr_output *wlr_output = nullptr;
    wlr_scene_output *scene_output = nullptr;
    wl_listener frame;
    wl_listener destroy;
};

struct HackMatrixView {
    HackMatrixWaylandServer *server = nullptr;
    wlr_xdg_surface *xdg_surface = nullptr;
    wlr_scene_tree *scene_tree = nullptr;
    wl_listener map;
    wl_listener unmap;
    wl_listener destroy;
};

struct HackMatrixWaylandServer {
    wl_display *display = nullptr;
    wlr_backend *backend = nullptr;
    wlr_renderer *renderer = nullptr;
    wlr_allocator *allocator = nullptr;
    wlr_compositor *compositor = nullptr;
    wlr_xdg_shell *xdg_shell = nullptr;
    wlr_scene *scene = nullptr;
    wlr_scene_rect *background_rect = nullptr;
    wlr_output_layout *output_layout = nullptr;
    wl_listener new_output;
    wl_listener new_xdg_surface;

    bool init();
    void run();
    void shutdown();

    HackMatrixWaylandServer() = default;
    HackMatrixWaylandServer(const HackMatrixWaylandServer &) = delete;
    HackMatrixWaylandServer &operator=(const HackMatrixWaylandServer &) = delete;
};

HackMatrixWaylandServer *g_active_server = nullptr;

void handle_sigint(int) {
    if (g_active_server && g_active_server->display) {
        wl_display_terminate(g_active_server->display);
    }
}

void handle_output_frame(struct wl_listener *listener, void *data) {
    (void)data;
    auto *hm_output = wl_container_of(listener, hm_output, frame);
    if (!hm_output->scene_output) {
        return;
    }

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    if (wlr_scene_output_commit(hm_output->scene_output)) {
        wlr_scene_output_send_frame_done(hm_output->scene_output, &now);
    }
}

void handle_output_destroy(struct wl_listener *listener, void *data) {
    (void)data;
    auto *hm_output = wl_container_of(listener, hm_output, destroy);
    wl_list_remove(&hm_output->frame.link);
    wl_list_remove(&hm_output->destroy.link);
    delete hm_output;
}

void handle_new_output(struct wl_listener *listener, void *data) {
    auto *server = wl_container_of(listener, server, new_output);
    auto *output = static_cast<wlr_output *>(data);

    if (!wlr_output_init_render(output, server->allocator, server->renderer)) {
        wlr_log(WLR_ERROR, "Failed to initialize render context for output %s", output->name);
        return;
    }

    if (!wl_list_empty(&output->modes)) {
        wlr_output_mode *preferred = wlr_output_preferred_mode(output);
        if (preferred) {
            wlr_output_set_mode(output, preferred);
        }
    }

    auto *hm_output = new HackMatrixOutput();
    hm_output->server = server;
    hm_output->wlr_output = output;
    hm_output->scene_output = wlr_scene_output_create(server->scene, output);
    hm_output->frame.notify = handle_output_frame;
    wl_signal_add(&output->events.frame, &hm_output->frame);
    hm_output->destroy.notify = handle_output_destroy;
    wl_signal_add(&output->events.destroy, &hm_output->destroy);

    if (server->background_rect) {
        const int width = output->width ? output->width : 1;
        const int height = output->height ? output->height : 1;
        wlr_scene_rect_set_size(server->background_rect, width, height);
    }

    wlr_output_layout_add_auto(server->output_layout, output);
    wlr_output_enable(output, true);
    if (!wlr_output_commit(output)) {
        wlr_log(WLR_ERROR, "Failed to commit output %s", output->name);
    }
}

void handle_view_map(struct wl_listener *listener, void *data) {
    (void)data;
    auto *view = wl_container_of(listener, view, map);
    wlr_scene_node_set_enabled(&view->scene_tree->node, true);
}

void handle_view_unmap(struct wl_listener *listener, void *data) {
    (void)data;
    auto *view = wl_container_of(listener, view, unmap);
    wlr_scene_node_set_enabled(&view->scene_tree->node, false);
}

void handle_view_destroy(struct wl_listener *listener, void *data) {
    (void)data;
    auto *view = wl_container_of(listener, view, destroy);
    wl_list_remove(&view->map.link);
    wl_list_remove(&view->unmap.link);
    wl_list_remove(&view->destroy.link);
    if (view->scene_tree) {
        wlr_scene_node_destroy(&view->scene_tree->node);
    }
    delete view;
}

void handle_new_xdg_surface(struct wl_listener *listener, void *data) {
    auto *server = wl_container_of(listener, server, new_xdg_surface);
    auto *xdg_surface = static_cast<wlr_xdg_surface *>(data);

    if (xdg_surface->role != WLR_XDG_SURFACE_ROLE_TOPLEVEL) {
        return;
    }

    auto *view = new HackMatrixView();
    view->server = server;
    view->xdg_surface = xdg_surface;
    view->scene_tree = wlr_scene_tree_create(&server->scene->tree);
    wlr_scene_node_set_enabled(&view->scene_tree->node, false);
    wlr_scene_xdg_surface_create(view->scene_tree, xdg_surface);

    view->map.notify = handle_view_map;
    wl_signal_add(&xdg_surface->events.map, &view->map);
    view->unmap.notify = handle_view_unmap;
    wl_signal_add(&xdg_surface->events.unmap, &view->unmap);
    view->destroy.notify = handle_view_destroy;
    wl_signal_add(&xdg_surface->events.destroy, &view->destroy);
}

bool HackMatrixWaylandServer::init() {
    wlr_log_init(WLR_DEBUG, nullptr);

    display = wl_display_create();
    if (!display) {
        std::fprintf(stderr, "Failed to create Wayland display\n");
        return false;
    }

    backend = wlr_backend_autocreate(display, nullptr);
    if (!backend) {
        std::fprintf(stderr, "Failed to create wlroots backend\n");
        return false;
    }

    renderer = wlr_renderer_autocreate(backend);
    if (!renderer) {
        std::fprintf(stderr, "Failed to create renderer\n");
        return false;
    }

    allocator = wlr_allocator_autocreate(renderer, backend);
    if (!allocator) {
        std::fprintf(stderr, "Failed to create allocator\n");
        return false;
    }

    compositor = wlr_compositor_create(display, renderer);
    if (!compositor) {
        std::fprintf(stderr, "Failed to create compositor\n");
        return false;
    }

    wlr_subcompositor_create(display);

    scene = wlr_scene_create();
    if (!scene) {
        std::fprintf(stderr, "Failed to create scene graph\n");
        return false;
    }

    background_rect = wlr_scene_rect_create(&scene->tree, 1, 1, kBackgroundColor);
    if (background_rect) {
        wlr_scene_node_lower_to_bottom(&background_rect->node);
    }

    output_layout = wlr_output_layout_create();
    if (!output_layout) {
        std::fprintf(stderr, "Failed to create output layout\n");
        return false;
    }

    xdg_shell = wlr_xdg_shell_create(display);
    if (!xdg_shell) {
        std::fprintf(stderr, "Failed to create xdg-shell interface\n");
        return false;
    }

    new_output.notify = handle_new_output;
    wl_signal_add(&backend->events.new_output, &new_output);
    new_xdg_surface.notify = handle_new_xdg_surface;
    wl_signal_add(&xdg_shell->events.new_surface, &new_xdg_surface);

    return true;
}

void HackMatrixWaylandServer::run() {
    const char *socket = wl_display_add_socket_auto(display);
    if (!socket) {
        std::fprintf(stderr, "Failed to add Wayland socket\n");
        return;
    }

    setenv("WAYLAND_DISPLAY", socket, true);

    if (!wlr_backend_start(backend)) {
        std::fprintf(stderr, "Failed to start wlroots backend\n");
        return;
    }

    wlr_log(WLR_INFO, "HackMatrix Wayland skeleton running on WAYLAND_DISPLAY=%s", socket);
    wl_display_run(display);
}

void HackMatrixWaylandServer::shutdown() {
    if (backend) {
        wlr_backend_destroy(backend);
        backend = nullptr;
    }
    if (allocator) {
        wlr_allocator_destroy(allocator);
        allocator = nullptr;
    }
    if (renderer) {
        wlr_renderer_destroy(renderer);
        renderer = nullptr;
    }
    if (display) {
        wl_display_destroy_clients(display);
    }
    if (scene) {
        wlr_scene_destroy(scene);
        scene = nullptr;
    }
    if (output_layout) {
        wlr_output_layout_destroy(output_layout);
        output_layout = nullptr;
    }
    if (display) {
        wl_display_destroy(display);
        display = nullptr;
    }
}

}  // namespace

int main() {
    HackMatrixWaylandServer server;
    g_active_server = &server;

    if (!server.init()) {
        return EXIT_FAILURE;
    }

    std::signal(SIGINT, handle_sigint);
    std::signal(SIGTERM, handle_sigint);

    server.run();
    server.shutdown();
    return EXIT_SUCCESS;
}
