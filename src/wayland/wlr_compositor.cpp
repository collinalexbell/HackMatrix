#define WLR_USE_UNSTABLE 1

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <pixman-1/pixman.h>
#include <memory>
#include <drm_fourcc.h>
#include <ctime>
#include <unordered_map>

extern "C" {
#include <EGL/egl.h>
#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/render/allocator.h>
#include <wlr/render/drm_format_set.h>
#include <wlr/render/gles2.h>
#include <wlr/render/swapchain.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_xcursor_manager.h>
#define namespace namespace_keyword_workaround
#include <wlr/types/wlr_layer_shell_v1.h>
#undef namespace
#include <wlr/types/wlr_xdg_output_v1.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_output.h>
#include <wlr/util/log.h>
#include <xkbcommon/xkbcommon.h>
}

#include <glad/glad.h>
#include <cstdarg>
#include <algorithm>
#include <string>

#include "engine.h"
#include "wayland_app.h"
#include "AppSurface.h"
#include "entity.h"
#include "screen.h"

static void
log_to_tmp(const char* fmt, ...)
{
  FILE* f = std::fopen("/tmp/matrix-wlroots-output.log", "a");
  if (!f) {
    return;
  }
  va_list args;
  va_start(args, fmt);
  std::vfprintf(f, fmt, args);
  std::fflush(f);
  va_end(args);
  std::fclose(f);
}

namespace {

double
currentTimeSeconds()
{
  static const auto start = std::chrono::steady_clock::now();
  const auto now = std::chrono::steady_clock::now();
  std::chrono::duration<double> elapsed = now - start;
  return elapsed.count();
}

struct WlrServer;

struct WlrOutputHandle {
  WlrServer* server = nullptr;
  wlr_output* output = nullptr;
  wlr_swapchain* swapchain = nullptr;
  wl_listener frame;
  wl_listener destroy;
  int buffer_age = -1;
  GLuint depth_rbo = 0;
  int depth_width = 0;
  int depth_height = 0;
};

struct WlrKeyboardHandle {
  WlrServer* server = nullptr;
  wlr_keyboard* keyboard = nullptr;
  wl_listener key;
  wl_listener destroy;
};

struct WlrPointerHandle {
  WlrServer* server = nullptr;
  wlr_pointer* pointer = nullptr;
  wl_listener motion;
  wl_listener motion_abs;
  wl_listener button;
  wl_listener destroy;
};

struct InputState {
  bool forward = false;
  bool back = false;
  bool left = false;
  bool right = false;
  double delta_x = 0.0;
  double delta_y = 0.0;
  bool have_abs = false;
  double last_abs_x = 0.0;
  double last_abs_y = 0.0;
};

struct PendingWlAction {
  enum Type { Add, Remove } type;
  std::shared_ptr<WaylandApp> app;
  wlr_surface* surface = nullptr;
};

struct WlrServer {
  wl_display* display = nullptr;
  wlr_backend* backend = nullptr;
  wlr_renderer* renderer = nullptr;
  wlr_allocator* allocator = nullptr;
  wl_listener new_output;
  wl_listener new_input;
  wl_listener new_xdg_surface;
  std::unique_ptr<Engine> engine;
  std::shared_ptr<EntityRegistry> registry;
  bool gladLoaded = false;
  double lastFrameTime = 0.0;
  char** envp = nullptr;
  InputState input;
  wlr_compositor* compositor = nullptr;
  wlr_xdg_shell* xdg_shell = nullptr;
  wlr_seat* seat = nullptr;
  wlr_data_device_manager* data_device_manager = nullptr;
  wlr_output_layout* output_layout = nullptr;
  wlr_xdg_output_manager_v1* xdg_output_manager = nullptr;
  wlr_layer_shell_v1* layer_shell = nullptr;
  wlr_output* primary_output = nullptr;
  wlr_input_device* last_keyboard_device = nullptr;
  wlr_input_device* last_pointer_device = nullptr;
  wlr_xcursor_manager* cursor_mgr = nullptr;
  wlr_cursor* cursor = nullptr;
  std::unordered_map<wlr_surface*, entt::entity> surface_map;
  std::vector<wlr_xdg_surface*> pending_surfaces;
  std::vector<PendingWlAction> pending_wl_actions;
  bool isX11Backend = false;
  double pointer_x = 0.0;
  double pointer_y = 0.0;
  wl_listener request_set_cursor;
};

static void
log_pointer_state(WlrServer* server, const char* tag)
{
  if (!server) {
    return;
  }
  double px = server->pointer_x;
  double py = server->pointer_y;
  if (server->cursor) {
    px = server->cursor->x;
    py = server->cursor->y;
    server->pointer_x = px;
    server->pointer_y = py;
  }
  wlr_surface* surf = nullptr;
  if (server->seat && server->seat->pointer_state.focused_surface) {
    surf = server->seat->pointer_state.focused_surface;
  }
  log_to_tmp("pointer[%s]: pos=(%.1f,%.1f) focus=%p\n",
             tag ? tag : "",
             px,
             py,
             (void*)surf);
}

static void
set_default_cursor(WlrServer* server)
{
  if (!server || !server->cursor || !server->cursor_mgr) {
    return;
  }
  // Load default theme if not already; reuse per-output scale later.
  wlr_xcursor_manager_load(server->cursor_mgr, 1);
  wlr_cursor_set_xcursor(server->cursor, server->cursor_mgr, "left_ptr");
  server->pointer_x = server->cursor->x;
  server->pointer_y = server->cursor->y;
  log_pointer_state(server, "default_cursor");
}

static void
ensure_wayland_apps_registered(WlrServer* server)
{
  if (!server || !server->registry || !server->engine) {
    return;
  }
  auto* renderer = server->engine->getRenderer();
  if (!renderer) {
    return;
  }
  FILE* f = std::fopen("/tmp/matrix-wlroots-output.log", "a");
  if (!server->pending_wl_actions.empty() && f) {
    std::fprintf(f,
                 "wayland deferred queue drain: count=%zu\n",
                 server->pending_wl_actions.size());
    std::fflush(f);
  }
  auto actions = std::move(server->pending_wl_actions);
  server->pending_wl_actions.clear();
  for (auto& action : actions) {
    if (action.type == PendingWlAction::Add) {
      entt::entity entity = entt::null;
      if (auto wm = server->engine->getWindowManager()) {
        entity = wm->registerWaylandApp(action.app, true);
        if (entity != entt::null) {
          if (auto* comp = server->registry->try_get<WaylandApp::Component>(entity)) {
            action.app = comp->app;
          }
        }
      }
      if (entity == entt::null && server->registry) {
        entity = server->registry->create();
        server->registry->emplace<WaylandApp::Component>(entity, action.app);
      }
      if (entity != entt::null) {
        renderer->registerApp(action.app.get());
        server->surface_map[action.surface] = entity;
        if (f) {
          std::fprintf(f,
                       "wayland app add (deferred): surface=%p ent=%d texId=%d texUnit=%d app=%p\n",
                       (void*)action.surface,
                       (int)entt::to_integral(entity),
                       action.app->getTextureId(),
                       action.app->getTextureUnit() - GL_TEXTURE0,
                       (void*)action.app.get());
          std::fflush(f);
        }
      } else {
        if (f) {
          std::fprintf(f,
                       "wayland app add (deferred) failed: surface=%p\n",
                       (void*)action.surface);
          std::fflush(f);
        }
      }
    } else if (action.type == PendingWlAction::Remove) {
      auto it = server->surface_map.find(action.surface);
      if (it != server->surface_map.end()) {
        entt::entity e = it->second;
        if (server->registry && server->registry->valid(e)) {
          if (auto* renderer = server->engine->getRenderer()) {
            if (auto* comp = server->registry->try_get<WaylandApp::Component>(e)) {
              if (comp->app) {
                renderer->deregisterApp((int)comp->app->getAppIndex());
              }
            }
          }
          server->registry->destroy(e);
        }
        server->surface_map.erase(it);
      }
      if (f) {
        std::fprintf(f,
                     "wayland app remove (deferred): surface=%p\n",
                     (void*)action.surface);
        std::fflush(f);
      }
    } else {
      if (f) {
        std::fprintf(f,
                     "wayland app action unknown (deferred): surface=%p\n",
                     (void*)action.surface);
        std::fflush(f);
      }
    }
  }
  if (f) {
    std::fclose(f);
  }
}

struct WaylandAppHandle {
  WlrServer* server = nullptr;
  std::shared_ptr<WaylandApp> app;
  wlr_surface* surface = nullptr;
  wl_listener destroy;
  wl_listener unmap;
  wl_listener commit;
  bool registered = false;
  entt::entity entity = entt::null;
};

struct XdgSurfaceHandle {
  WlrServer* server = nullptr;
  wlr_xdg_surface* xdg = nullptr;
  bool created = false;
  bool configured_sent = false;
  wl_listener commit;
  wl_listener map;
  wl_listener destroy;
};

WlrServer* g_server = nullptr;

static bool
wayland_app_has_pointer_focus(WlrServer* server)
{
  if (!server || !server->engine) {
    return false;
  }
  auto wm = server->engine->getWindowManager();
  if (!wm || !server->registry) {
    return false;
  }
  auto focused = wm->getCurrentlyFocusedApp();
  if (!focused.has_value() || !server->registry->valid(*focused)) {
    return false;
  }
  auto entity = focused.value();
  if (!server->registry->all_of<WaylandApp::Component>(entity)) {
    return false;
  }
  auto& comp = server->registry->get<WaylandApp::Component>(entity);
  wlr_surface* focused_surface = comp.app ? comp.app->getSurface() : nullptr;
  if (!focused_surface) {
    return false;
  }
  if (server->seat && server->seat->pointer_state.focused_surface) {
    return server->seat->pointer_state.focused_surface == focused_surface;
  }
  // Fallback: if we can't inspect seat focus, honor the WM focus flag.
  return true;
}

static bool
pick_output_size(bool isX11Backend, int* out_width, int* out_height)
{
  // Order of preference:
  // 1) Explicit SCREEN_WIDTH/HEIGHT envs.
  // 2) For X11 backend, use WLR_X11_OUTPUT_{WIDTH,HEIGHT} if set.
  // 3) Default (no override) => let backend choose.
  int width = 0;
  int height = 0;

  if (const char* w_env = std::getenv("SCREEN_WIDTH")) {
    width = std::atoi(w_env);
  }
  if (const char* h_env = std::getenv("SCREEN_HEIGHT")) {
    height = std::atoi(h_env);
  }

  if (width <= 0 || height <= 0) {
    if (isX11Backend) {
      if (const char* wx = std::getenv("WLR_X11_OUTPUT_WIDTH")) {
        width = std::atoi(wx);
      }
      if (const char* hx = std::getenv("WLR_X11_OUTPUT_HEIGHT")) {
        height = std::atoi(hx);
      }
      if (width <= 0 || height <= 0) {
        width = 1920;
        height = 1080;
      }
    } else {
      return false;
    }
  }

  *out_width = width;
  *out_height = height;
  return true;
}

void
handle_keyboard_destroy(wl_listener* listener, void* data)
{
  (void)data;
  auto* handle =
    wl_container_of(listener, static_cast<WlrKeyboardHandle*>(nullptr), destroy);
  wl_list_remove(&handle->key.link);
  wl_list_remove(&handle->destroy.link);
  delete handle;
}

void
handle_keyboard_key(wl_listener* listener, void* data)
{
  auto* handle =
    wl_container_of(listener, static_cast<WlrKeyboardHandle*>(nullptr), key);
  auto* server = handle->server;
  auto* event = static_cast<wlr_keyboard_key_event*>(data);
  uint32_t keycode = event->keycode + 8;
  const xkb_keysym_t* syms;
  int nsyms = xkb_state_key_get_syms(handle->keyboard->xkb_state, keycode, &syms);
  for (int i = 0; i < nsyms; ++i) {
    if (syms[i] == XKB_KEY_Escape && event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
      wl_display_terminate(server->display);
    }
    bool pressed = event->state == WL_KEYBOARD_KEY_STATE_PRESSED;
    switch (syms[i]) {
      case XKB_KEY_w:
      case XKB_KEY_W:
        server->input.forward = pressed;
        break;
      case XKB_KEY_s:
      case XKB_KEY_S:
        server->input.back = pressed;
        break;
      case XKB_KEY_a:
      case XKB_KEY_A:
        server->input.left = pressed;
        break;
      case XKB_KEY_d:
      case XKB_KEY_D:
        server->input.right = pressed;
        break;
      case XKB_KEY_r:
      case XKB_KEY_R:
        if (pressed && server->engine) {
          if (auto wm = server->engine->getWindowManager()) {
            wm->focusLookedAtApp();
          }
        }
        break;
      case XKB_KEY_v:
      case XKB_KEY_V:
        if (pressed && server->engine) {
          if (auto wm = server->engine->getWindowManager()) {
            wm->menu();
          }
        }
        break;
      default:
        break;
    }
    FILE* f = std::fopen("/tmp/matrix-wlroots-wm.log", "a");
    if (f) {
      std::fprintf(f, "key sym=%u pressed=%d\n", syms[i], pressed ? 1 : 0);
      std::fflush(f);
      std::fclose(f);
    }
    wlr_log(WLR_DEBUG, "key sym=%u pressed=%d", syms[i], pressed ? 1 : 0);
  }
  if (server->seat) {
    // Only forward keys to the app when the focused surface matches the WM's
    // focused Wayland app.
    wlr_surface* focused_surface = server->seat->keyboard_state.focused_surface;
    bool deliverToApp = false;
    if (server->engine) {
      if (auto wm = server->engine->getWindowManager()) {
        if (auto focused = wm->getCurrentlyFocusedApp()) {
          if (server->registry &&
              server->registry->all_of<WaylandApp::Component>(*focused)) {
            auto& comp = server->registry->get<WaylandApp::Component>(*focused);
            if (comp.app && comp.app->getSurface() == focused_surface) {
              deliverToApp = true;
            }
          }
        }
      }
    }
    if (deliverToApp) {
      wlr_seat_set_keyboard(server->seat, handle->keyboard);
      enum wl_keyboard_key_state state = event->state;
      wlr_seat_keyboard_notify_key(
        server->seat, event->time_msec, event->keycode, state);
      wlr_seat_keyboard_notify_modifiers(server->seat, &handle->keyboard->modifiers);
    }
  }
}
}

void
handle_new_input(wl_listener* listener, void* data)
{
  auto* server =
    wl_container_of(listener, static_cast<WlrServer*>(nullptr), new_input);
  auto* device = static_cast<wlr_input_device*>(data);
  if (device->type != WLR_INPUT_DEVICE_KEYBOARD) {
    if (device->type == WLR_INPUT_DEVICE_POINTER) {
      auto* pointer = wlr_pointer_from_input_device(device);
      server->last_pointer_device = device;
      if (server->cursor) {
        wlr_cursor_attach_input_device(server->cursor, device);
        // Ensure a visible default cursor when a new pointer arrives.
        set_default_cursor(server);
      }
      auto* handle = new WlrPointerHandle();
      handle->server = server;
      handle->pointer = pointer;
      handle->motion.notify = [](wl_listener* listener, void* data) {
        auto* handle =
          wl_container_of(listener, static_cast<WlrPointerHandle*>(nullptr), motion);
        auto* event = static_cast<wlr_pointer_motion_event*>(data);
        handle->server->input.delta_x += event->delta_x;
        handle->server->input.delta_y += event->delta_y;
        if (handle->server->cursor) {
          wlr_cursor_move(handle->server->cursor,
                          handle->server->last_pointer_device,
                          event->delta_x,
                          event->delta_y);
          handle->server->pointer_x = handle->server->cursor->x;
          handle->server->pointer_y = handle->server->cursor->y;
        }
        if (handle->server->seat) {
          wlr_seat_pointer_notify_motion(handle->server->seat,
                                         event->time_msec,
                                         handle->server->pointer_x,
                                         handle->server->pointer_y);
        }
        log_pointer_state(handle->server, "motion_rel");
        wlr_log(WLR_DEBUG, "pointer motion rel dx=%.3f dy=%.3f",
                event->delta_x,
                event->delta_y);
      };
      wl_signal_add(&pointer->events.motion, &handle->motion);
      handle->motion_abs.notify = [](wl_listener* listener, void* data) {
        auto* handle =
          wl_container_of(listener, static_cast<WlrPointerHandle*>(nullptr), motion_abs);
        auto* event = static_cast<wlr_pointer_motion_absolute_event*>(data);
        if (handle->server->input.have_abs) {
          // Normalize deltas into something closer to pixel units.
          double dx = (event->x - handle->server->input.last_abs_x) * 1000.0;
          double dy = (event->y - handle->server->input.last_abs_y) * 1000.0;
          handle->server->input.delta_x += dx;
          handle->server->input.delta_y += dy;
        }
        handle->server->input.have_abs = true;
        handle->server->input.last_abs_x = event->x;
        handle->server->input.last_abs_y = event->y;
        if (handle->server->cursor) {
          wlr_cursor_warp_absolute(handle->server->cursor,
                                   handle->server->last_pointer_device,
                                   event->x,
                                   event->y);
          handle->server->pointer_x = handle->server->cursor->x;
          handle->server->pointer_y = handle->server->cursor->y;
        }
        if (handle->server->seat) {
          wlr_seat_pointer_notify_motion(handle->server->seat,
                                         event->time_msec,
                                         handle->server->pointer_x,
                                         handle->server->pointer_y);
        }
        log_pointer_state(handle->server, "motion_abs");
        FILE* f = std::fopen("/tmp/matrix-wlroots-wm.log", "a");
        if (f) {
          std::fprintf(f, "pointer motion abs dx=%.3f dy=%.3f\n",
                       handle->server->input.delta_x,
                       handle->server->input.delta_y);
          std::fflush(f);
          std::fclose(f);
        }
        wlr_log(WLR_DEBUG, "pointer motion abs dx=%.3f dy=%.3f", handle->server->input.delta_x, handle->server->input.delta_y);
      };
      wl_signal_add(&pointer->events.motion_absolute, &handle->motion_abs);
      handle->button.notify = [](wl_listener* listener, void* data) {
        auto* handle =
          wl_container_of(listener, static_cast<WlrPointerHandle*>(nullptr), button);
        auto* event = static_cast<wlr_pointer_button_event*>(data);
        if (event->state == WLR_BUTTON_PRESSED && handle->server && handle->server->engine) {
          if (auto wm = handle->server->engine->getWindowManager()) {
            FILE* f = std::fopen("/tmp/matrix-wlroots-wm.log", "a");
            if (f) {
              std::fprintf(f, "wlr: pointer button press -> focusLookedAtApp\n");
              std::fflush(f);
              std::fclose(f);
            }
            wm->focusLookedAtApp();
          }
        }
        if (handle->server->seat) {
          wlr_seat_pointer_notify_button(handle->server->seat,
                                         event->time_msec,
                                         event->button,
                                         event->state);
        }
      };
      wl_signal_add(&pointer->events.button, &handle->button);
      handle->destroy.notify = [](wl_listener* listener, void* data) {
        (void)data;
        auto* handle =
          wl_container_of(listener, static_cast<WlrPointerHandle*>(nullptr), destroy);
        wl_list_remove(&handle->motion.link);
        wl_list_remove(&handle->motion_abs.link);
        wl_list_remove(&handle->button.link);
        wl_list_remove(&handle->destroy.link);
        delete handle;
      };
      wl_signal_add(&device->events.destroy, &handle->destroy);
    }
    return;
  }

  auto* keyboard = wlr_keyboard_from_input_device(device);
  server->last_keyboard_device = device;
  if (server->seat) {
    wlr_seat_set_capabilities(server->seat,
                              WL_SEAT_CAPABILITY_POINTER | WL_SEAT_CAPABILITY_KEYBOARD);
    wlr_seat_set_keyboard(server->seat, keyboard);
  }
  auto* handle = new WlrKeyboardHandle();
  handle->server = server;
  handle->keyboard = keyboard;
  handle->key.notify = [](wl_listener* listener, void* data) {
    auto* handle =
      wl_container_of(listener, static_cast<WlrKeyboardHandle*>(nullptr), key);
    auto* event = static_cast<wlr_keyboard_key_event*>(data);
    FILE* f = std::fopen("/tmp/matrix-wlroots-wm.log", "a");
    if (f) {
      std::fprintf(f,
                   "wlr: key time=%u keycode=%u state=%u device=%p\n",
                   event->time_msec,
                   event->keycode,
                   event->state,
                   (void*)handle->keyboard);
      std::fflush(f);
      std::fclose(f);
    }
    handle_keyboard_key(listener, data);
  };
  wl_signal_add(&keyboard->events.key, &handle->key);
  handle->destroy.notify = handle_keyboard_destroy;
  wl_signal_add(&device->events.destroy, &handle->destroy);

  xkb_context* context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
  xkb_keymap* keymap =
    xkb_keymap_new_from_names(context, nullptr, XKB_KEYMAP_COMPILE_NO_FLAGS);
  wlr_keyboard_set_keymap(keyboard, keymap);
  xkb_keymap_unref(keymap);
  xkb_context_unref(context);
  wlr_keyboard_set_repeat_info(keyboard, 25, 600);
  if (server->seat) {
    wlr_seat_keyboard_notify_modifiers(server->seat, &keyboard->modifiers);
    if (server->seat->keyboard_state.focused_surface) {
      wlr_seat_keyboard_notify_enter(server->seat,
                                     server->seat->keyboard_state.focused_surface,
                                     keyboard->keycodes,
                                     keyboard->num_keycodes,
                                     &keyboard->modifiers);
    }
  }
}

static void create_wayland_app(WlrServer* server, wlr_xdg_surface* xdg_surface)
{
  auto app = std::make_shared<WaylandApp>(server->renderer,
                                          server->allocator,
                                          xdg_surface,
                                          0);
  app->setSeat(server->seat, xdg_surface->surface);

  auto* handle = new WaylandAppHandle();
  handle->server = server;
  handle->app = app;
  handle->surface = xdg_surface->surface;
  handle->entity = entt::null;
  handle->registered = false;
  handle->commit.notify = [](wl_listener* listener, void* data) {
    auto* handle = wl_container_of(listener, static_cast<WaylandAppHandle*>(nullptr), commit);
    auto* surf = static_cast<wlr_surface*>(data);
    if (!handle->server || !surf) {
      return;
    }
    int w = surf->current.width;
    int h = surf->current.height;
    if (w <= 0 || h <= 0) {
      return;
    }
    // Defer registration to the render loop to avoid GL/context races.
    handle->server->pending_wl_actions.push_back(
      PendingWlAction{ PendingWlAction::Add, handle->app, surf });
    log_to_tmp("wayland deferred add queued: surface=%p size=%dx%d app=%p\n",
               (void*)surf,
               w,
               h,
               (void*)handle->app.get());
  };
  wl_signal_add(&xdg_surface->surface->events.commit, &handle->commit);
  handle->unmap.notify = [](wl_listener* listener, void* /*data*/) {
    auto* handle = wl_container_of(listener, static_cast<WaylandAppHandle*>(nullptr), unmap);
    if (handle->server && handle->surface) {
      handle->server->pending_wl_actions.push_back(
        PendingWlAction{ PendingWlAction::Remove, handle->app, handle->surface });
      log_to_tmp("wayland deferred remove queued (unmap): surface=%p app=%p\n",
                 (void*)handle->surface,
                 (void*)handle->app.get());
    }
  };
  wl_signal_add(&xdg_surface->surface->events.unmap, &handle->unmap);
  handle->destroy.notify = [](wl_listener* listener, void* data) {
    auto* handle = wl_container_of(listener, static_cast<WaylandAppHandle*>(nullptr), destroy);
    auto* surf = static_cast<wlr_surface*>(data);
    if (handle->server) {
      handle->server->pending_wl_actions.push_back(
        PendingWlAction{ PendingWlAction::Remove, handle->app, surf });
      log_to_tmp("wayland deferred remove queued (destroy): surface=%p app=%p\n",
                 (void*)surf,
                 (void*)handle->app.get());
    }
    wl_list_remove(&handle->destroy.link);
    wl_list_remove(&handle->commit.link);
    delete handle;
  };
  wl_signal_add(&xdg_surface->surface->events.destroy, &handle->destroy);

  if (server->primary_output) {
    wlr_surface_send_enter(xdg_surface->surface, server->primary_output);
  }
  if (server->seat && server->last_keyboard_device) {
    auto* kbd = wlr_keyboard_from_input_device(server->last_keyboard_device);
    wlr_seat_set_keyboard(server->seat, kbd);
    wlr_seat_keyboard_notify_enter(server->seat,
                                   xdg_surface->surface,
                                   kbd->keycodes,
                                   kbd->num_keycodes,
                                   &kbd->modifiers);
    wlr_seat_keyboard_notify_modifiers(server->seat, &kbd->modifiers);
  }
  if (server->seat) {
    wlr_seat_pointer_notify_enter(server->seat, xdg_surface->surface, 0.0, 0.0);
  }
}

void
handle_new_xdg_surface(wl_listener* listener, void* data)
{
  auto* server = wl_container_of(listener, static_cast<WlrServer*>(nullptr), new_xdg_surface);
  auto* xdg_surface = static_cast<wlr_xdg_surface*>(data);
  wlr_log(WLR_DEBUG, "new xdg_surface %p", (void*)xdg_surface);
  if (xdg_surface->role == WLR_XDG_SURFACE_ROLE_TOPLEVEL && xdg_surface->toplevel) {
    wlr_xdg_toplevel_set_size(xdg_surface->toplevel, 1280, 720);
    wlr_xdg_toplevel_set_activated(xdg_surface->toplevel, true);
    wlr_xdg_surface_schedule_configure(xdg_surface);
  }

  auto* handle = new XdgSurfaceHandle();
  handle->server = server;
  handle->xdg = xdg_surface;

  handle->commit.notify = [](wl_listener* listener, void* data) {
    auto* handle = wl_container_of(listener, static_cast<XdgSurfaceHandle*>(nullptr), commit);
    auto* surface = static_cast<wlr_surface*>(data);
    if (!handle->server || !handle->xdg) {
      return;
    }
    wlr_log(WLR_DEBUG,
            "xdg_surface %p commit role=%d mapped=%d size=%dx%d",
            (void*)handle->xdg,
            handle->xdg->role,
            surface->mapped ? 1 : 0,
            surface->current.width,
            surface->current.height);
    if (handle->xdg->role != WLR_XDG_SURFACE_ROLE_TOPLEVEL) {
      return;
    }
    if (!handle->configured_sent) {
      if (handle->xdg->toplevel) {
        wlr_xdg_toplevel_set_size(handle->xdg->toplevel, 1280, 720);
        wlr_xdg_toplevel_set_activated(handle->xdg->toplevel, true);
      }
      wlr_log(WLR_DEBUG, "xdg_surface %p initial configure", (void*)handle->xdg);
      wlr_xdg_surface_schedule_configure(handle->xdg);
      handle->configured_sent = true;
    }
    if (surface->mapped && !handle->created) {
      handle->created = true;
      create_wayland_app(handle->server, handle->xdg);
    }
    (void)surface;
  };
  wl_signal_add(&xdg_surface->surface->events.commit, &handle->commit);

  handle->map.notify = [](wl_listener* listener, void* /*data*/) {
    auto* handle = wl_container_of(listener, static_cast<XdgSurfaceHandle*>(nullptr), map);
    if (!handle || !handle->server || !handle->xdg) {
      return;
    }
    wlr_log(WLR_DEBUG, "xdg_surface %p mapped", (void*)handle->xdg);
    if (!handle->created) {
      handle->created = true;
      create_wayland_app(handle->server, handle->xdg);
    }
  };
  wl_signal_add(&xdg_surface->surface->events.map, &handle->map);

  handle->destroy.notify = [](wl_listener* listener, void* data) {
    auto* handle = wl_container_of(listener, static_cast<XdgSurfaceHandle*>(nullptr), destroy);
    auto* xdg = static_cast<wlr_xdg_surface*>(data);
    (void)xdg;
    handle->xdg = nullptr;
    wl_list_remove(&handle->map.link);
    wl_list_remove(&handle->commit.link);
    wl_list_remove(&handle->destroy.link);
    delete handle;
  };
  wl_signal_add(&xdg_surface->events.destroy, &handle->destroy);
}

static void
handle_new_layer_surface(struct wl_listener* listener, void* data)
{
  // Minimal layer-shell support: do nothing. We don't present layer surfaces
  // yet, but ignoring them avoids crashing on premature configure.
  (void)listener;
  (void)data;
}

void
handle_output_destroy(wl_listener* listener, void* data)
{
  (void)data;
  auto* handle =
    wl_container_of(listener, static_cast<WlrOutputHandle*>(nullptr), destroy);
  if (handle->server && handle->server->output_layout && handle->output) {
    wlr_output_layout_remove(handle->server->output_layout, handle->output);
  }
  wl_list_remove(&handle->frame.link);
  wl_list_remove(&handle->destroy.link);
  if (handle->swapchain) {
    wlr_swapchain_destroy(handle->swapchain);
  }
  if (handle->depth_rbo) {
    glDeleteRenderbuffers(1, &handle->depth_rbo);
    handle->depth_rbo = 0;
  }
  delete handle;
}

void
ensure_glad(WlrServer* server)
{
  if (server->gladLoaded) {
    return;
  }
  if (!gladLoadGLLoader((GLADloadproc)eglGetProcAddress)) {
    wlr_log(WLR_ERROR, "Failed to load GL functions via EGL");
    wl_display_terminate(server->display);
    return;
  }
  const char* glVersion = reinterpret_cast<const char*>(glGetString(GL_VERSION));
  const char* glslVersion =
    reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION));
  std::fprintf(stderr, "wlroots compositor GL version: %s, GLSL: %s\n",
               glVersion ? glVersion : "(null)",
               glslVersion ? glslVersion : "(null)");
  server->gladLoaded = true;
}

void
ensure_depth_buffer(WlrOutputHandle* handle, int width, int height, GLuint fbo)
{
  if (handle->depth_rbo != 0 &&
      handle->depth_width == width &&
      handle->depth_height == height) {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,
                              GL_DEPTH_ATTACHMENT,
                              GL_RENDERBUFFER,
                              handle->depth_rbo);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,
                              GL_STENCIL_ATTACHMENT,
                              GL_RENDERBUFFER,
                              handle->depth_rbo);
    return;
  }

  if (handle->depth_rbo != 0) {
    glDeleteRenderbuffers(1, &handle->depth_rbo);
  }
  glGenRenderbuffers(1, &handle->depth_rbo);
  glBindRenderbuffer(GL_RENDERBUFFER, handle->depth_rbo);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER,
                            GL_DEPTH_STENCIL_ATTACHMENT,
                            GL_RENDERBUFFER,
                            handle->depth_rbo);
  handle->depth_width = width;
  handle->depth_height = height;
  GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  if (status != GL_FRAMEBUFFER_COMPLETE) {
    std::fprintf(stderr,
                 "FBO incomplete after attaching depth: 0x%x (size %dx%d)\n",
                 status,
                 width,
                 height);
  }
}

void
handle_output_frame(wl_listener* listener, void* data)
{
  (void)data;
  auto* handle =
    wl_container_of(listener, static_cast<WlrOutputHandle*>(nullptr), frame);
  auto* server = handle->server;

  struct wlr_output_state output_state;
  wlr_output_state_init(&output_state);

  if (!wlr_output_configure_primary_swapchain(
        handle->output, &output_state, &handle->swapchain)) {
    wlr_log(WLR_ERROR, "Failed to configure primary swapchain");
    wlr_output_state_finish(&output_state);
    return;
  }

  wlr_buffer* buffer = wlr_swapchain_acquire(handle->swapchain);
  if (!buffer) {
    wlr_log(WLR_ERROR, "Failed to acquire buffer from swapchain");
    wlr_output_state_finish(&output_state);
    return;
  }

  const wlr_buffer_pass_options pass_options = {};
  wlr_render_pass* pass =
    wlr_renderer_begin_buffer_pass(server->renderer, buffer, &pass_options);
  if (!pass) {
    wlr_log(WLR_ERROR, "Failed to begin buffer pass");
    wlr_buffer_unlock(buffer);
    wlr_output_state_finish(&output_state);
    return;
  }

  int width = handle->swapchain ? handle->swapchain->width : 1;
  int height = handle->swapchain ? handle->swapchain->height : 1;
  log_to_tmp("frame: swapchain %dx%d output %s\n",
             width,
             height,
             handle->output ? handle->output->name : "(null)");

  ensure_glad(server);

  GLuint fbo = wlr_gles2_renderer_get_buffer_fbo(server->renderer, buffer);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  ensure_depth_buffer(handle, width, height, fbo);
  if (!server->engine) {
    EngineOptions options;
    options.enableControls = false; // wlroots path feeds input directly
    options.enableGui = false;
    options.invertYAxis = true;
    server->engine = std::make_unique<Engine>(nullptr, server->envp, options);
  }
  if (server->engine && !server->registry) {
    server->registry = server->engine->getRegistry();
  }
  // Ensure Wayland apps have textures attached once the renderer exists.
  ensure_wayland_apps_registered(server);

  if (server->engine) {
    Camera* camera = server->engine->getCamera();
    if (camera) {
      camera->handleTranslateForce(
        server->input.forward, server->input.back, server->input.left, server->input.right);
      bool pointerOwnedByApp = wayland_app_has_pointer_focus(server);
      if (!pointerOwnedByApp &&
          (server->input.delta_x != 0.0 || server->input.delta_y != 0.0)) {
        camera->handleRotateForce(
          nullptr, server->input.delta_x, -server->input.delta_y);
      }
      // Always clear accumulated deltas so they don't apply after focus changes.
      server->input.delta_x = 0.0;
      server->input.delta_y = 0.0;
    }
  }

  if (server->engine && server->registry && !server->pending_surfaces.empty()) {
    auto pending = std::move(server->pending_surfaces);
    server->pending_surfaces.clear();
    for (auto* xdg : pending) {
      create_wayland_app(server, xdg);
    }
  }

  glViewport(0, 0, width, height);
  log_to_tmp("frame: glViewport %dx%d\n", width, height);
  double frameStart = currentTimeSeconds();
  server->engine->frame(frameStart);

  // Render software cursors (clients set them via wl_pointer.set_cursor).
  pixman_region32_t cursor_damage;
  pixman_region32_init_rect(&cursor_damage, 0, 0, width, height);
  wlr_output_add_software_cursors_to_render_pass(handle->output, pass, &cursor_damage);
  pixman_region32_fini(&cursor_damage);
  log_pointer_state(server, "frame");

  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  wlr_render_pass_submit(pass);

  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  for (auto& entry : server->surface_map) {
    wlr_surface* root = entry.first;
    wlr_surface_for_each_surface(
      root,
      [](wlr_surface* surface, int, int, void* data) {
        auto* ts = static_cast<timespec*>(data);
        wlr_surface_send_frame_done(surface, ts);
      },
      &now);
  }

  wlr_output_state_set_buffer(&output_state, buffer);
  if (!wlr_output_commit_state(handle->output, &output_state)) {
    wlr_log(WLR_ERROR, "Failed to commit output frame");
  }
  wlr_output_state_finish(&output_state);
  wlr_buffer_unlock(buffer);
  server->lastFrameTime = frameStart;
}

void
handle_new_output(wl_listener* listener, void* data)
{
  auto* server =
    wl_container_of(listener, static_cast<WlrServer*>(nullptr), new_output);
  auto* output = static_cast<wlr_output*>(data);

  if (!wlr_output_init_render(output, server->allocator, server->renderer)) {
    wlr_log(WLR_ERROR, "Failed to init render for output %s", output->name);
    return;
  }

  if (server->output_layout) {
    wlr_output_layout_add_auto(server->output_layout, output);
  }

  struct wlr_output_state state;
  wlr_output_state_init(&state);
  wlr_output_state_set_enabled(&state, true);
  int desired_width = 0;
  int desired_height = 0;
  bool have_override =
    pick_output_size(server->isX11Backend, &desired_width, &desired_height);
  if (have_override) {
    int refresh = server->isX11Backend ? 0 : 60000; // X11 backend ignores refresh
    if (wlr_output_mode* mode = wlr_output_preferred_mode(output)) {
      refresh = mode->refresh;
    }
    wlr_output_state_set_custom_mode(
      &state, desired_width, desired_height, refresh);
    log_to_tmp("new_output: override mode %dx%d@%d on %s\n",
               desired_width,
               desired_height,
               refresh,
               output->name ? output->name : "(null)");
  } else if (wlr_output_mode* mode = wlr_output_preferred_mode(output)) {
    wlr_output_state_set_mode(&state, mode);
    log_to_tmp("new_output: preferred mode %dx%d@%d on %s\n",
               mode->width,
               mode->height,
               mode->refresh,
               output->name ? output->name : "(null)");
  }
  if (!wlr_output_commit_state(output, &state)) {
    wlr_log(WLR_ERROR, "Failed to commit output %s", output->name);
    wlr_output_state_finish(&state);
    log_to_tmp("new_output: commit failed %s\n", output->name ? output->name : "(null)");
    return;
  }
  wlr_output_state_finish(&state);

  // Keep global screen dimensions in sync with the wlroots output so renderer
  // projection and texture sizing match the real buffer size.
  SCREEN_WIDTH = static_cast<float>(output->width);
  SCREEN_HEIGHT = static_cast<float>(output->height);
  log_to_tmp("new_output: committed size %dx%d (SCREEN %fx%f) name=%s\n",
             output->width,
             output->height,
             SCREEN_WIDTH,
             SCREEN_HEIGHT,
             output->name ? output->name : "(null)");

  auto* handle = new WlrOutputHandle();
  handle->server = server;
  handle->output = output;
  server->pointer_x = output->width / 2.0;
  server->pointer_y = output->height / 2.0;
  if (server->cursor) {
    wlr_cursor_map_to_output(server->cursor, output);
    wlr_cursor_warp(server->cursor, nullptr, server->pointer_x, server->pointer_y);
    if (server->cursor_mgr) {
      wlr_xcursor_manager_load(server->cursor_mgr, output->scale);
      set_default_cursor(server);
    }
  }
  if (!server->primary_output) {
    server->primary_output = output;
  }
  for (auto& entry : server->surface_map) {
    wlr_surface_send_enter(entry.first, output);
  }
  wlr_log(WLR_DEBUG, "new output %s %dx%d", output->name, output->width, output->height);
  handle->frame.notify = handle_output_frame;
  wl_signal_add(&output->events.frame, &handle->frame);
  handle->destroy.notify = handle_output_destroy;
  wl_signal_add(&output->events.destroy, &handle->destroy);

  wlr_output_schedule_frame(output);
}

int
main(int argc, char** argv, char** envp)
{
  (void)argc;
  (void)argv;
  WlrServer server = {};
  server.envp = envp;

  wlr_log_init(WLR_DEBUG, nullptr);
  log_to_tmp("startup: WLR_BACKENDS=%s DISPLAY=%s WAYLAND_DISPLAY=%s\n",
             std::getenv("WLR_BACKENDS") ? std::getenv("WLR_BACKENDS") : "(null)",
             std::getenv("DISPLAY") ? std::getenv("DISPLAY") : "(null)",
             std::getenv("WAYLAND_DISPLAY") ? std::getenv("WAYLAND_DISPLAY") : "(null)");
  log_to_tmp("startup: SCREEN_WIDTH=%s SCREEN_HEIGHT=%s\n",
             std::getenv("SCREEN_WIDTH") ? std::getenv("SCREEN_WIDTH") : "(null)",
             std::getenv("SCREEN_HEIGHT") ? std::getenv("SCREEN_HEIGHT") : "(null)");
  log_to_tmp("startup: XDG_RUNTIME_DIR=%s\n",
             std::getenv("XDG_RUNTIME_DIR") ? std::getenv("XDG_RUNTIME_DIR") : "(null)");

  // If WLR_BACKENDS not set, prefer wayland when under Wayland, otherwise try X11
  // so running from an X session or tty can still work.
  if (!std::getenv("WLR_BACKENDS")) {
    if (std::getenv("WAYLAND_DISPLAY")) {
      setenv("WLR_BACKENDS", "wayland", 1);
    } else if (std::getenv("DISPLAY")) {
      setenv("WLR_BACKENDS", "x11", 1);
    }
  }
  // For the X11 backend, wlroots expects an explicit output size. Provide one
  // (default 1920x1080) unless the user already set WLR_X11_OUTPUT_*.
  const char* backends_env = std::getenv("WLR_BACKENDS");
  bool likely_x11 = (backends_env && std::string(backends_env).find("x11") != std::string::npos) ||
                    (!std::getenv("WAYLAND_DISPLAY") && std::getenv("DISPLAY"));
  if (likely_x11) {
    if (!std::getenv("WLR_X11_OUTPUT_WIDTH") ||
        !std::getenv("WLR_X11_OUTPUT_HEIGHT")) {
      int width = 1920;
      int height = 1080;
      if (const char* w_env = std::getenv("SCREEN_WIDTH")) {
        width = std::max(1, std::atoi(w_env));
      }
      if (const char* h_env = std::getenv("SCREEN_HEIGHT")) {
        height = std::max(1, std::atoi(h_env));
      }
      char buf[32];
      std::snprintf(buf, sizeof(buf), "%d", width);
      setenv("WLR_X11_OUTPUT_WIDTH", buf, 1);
      std::snprintf(buf, sizeof(buf), "%d", height);
      setenv("WLR_X11_OUTPUT_HEIGHT", buf, 1);
      log_to_tmp("startup: set WLR_X11_OUTPUT_WIDTH=%d HEIGHT=%d\n", width, height);
    }
  }

  server.display = wl_display_create();
  if (!server.display) {
    std::fprintf(stderr, "Failed to create Wayland display\n");
    return EXIT_FAILURE;
  }
  g_server = &server;

  server.backend =
    wlr_backend_autocreate(wl_display_get_event_loop(server.display), nullptr);
  if (!server.backend) {
    std::fprintf(stderr, "Failed to create wlroots backend\n");
    return EXIT_FAILURE;
  }

  server.renderer = wlr_renderer_autocreate(server.backend);
  if (!server.renderer) {
    std::fprintf(stderr, "Failed to create renderer\n");
    return EXIT_FAILURE;
  }
  if (!wlr_renderer_init_wl_display(server.renderer, server.display)) {
    std::fprintf(stderr, "Failed to init renderer for wl_display\n");
    return EXIT_FAILURE;
  }

  server.allocator = wlr_allocator_autocreate(server.backend, server.renderer);
  if (!server.allocator) {
    std::fprintf(stderr, "Failed to create allocator\n");
    return EXIT_FAILURE;
  }
  // Detect X11 backend (common for nested testing).
  const char* backend_env = std::getenv("WLR_BACKENDS");
  if (backend_env && std::string(backend_env).find("x11") != std::string::npos) {
    server.isX11Backend = true;
  } else if (!std::getenv("WAYLAND_DISPLAY") && std::getenv("DISPLAY")) {
    server.isX11Backend = true;
  }

  server.output_layout = wlr_output_layout_create(server.display);

  server.compositor = wlr_compositor_create(server.display, 5, server.renderer);
  if (!server.compositor) {
    std::fprintf(stderr, "Failed to create compositor\n");
    return EXIT_FAILURE;
  }
  // Some clients (e.g., foot) require wl_subcompositor to be advertised.
  if (!wlr_subcompositor_create(server.display)) {
    std::fprintf(stderr, "Failed to create subcompositor\n");
    return EXIT_FAILURE;
  }
  server.xdg_shell = wlr_xdg_shell_create(server.display, 3);
  if (!server.xdg_shell) {
    std::fprintf(stderr, "Failed to create xdg-shell\n");
    return EXIT_FAILURE;
  }
  server.seat = wlr_seat_create(server.display, "seat0");
  if (server.seat) {
    wlr_seat_set_capabilities(server.seat,
                              WL_SEAT_CAPABILITY_POINTER | WL_SEAT_CAPABILITY_KEYBOARD);
    server.cursor_mgr = wlr_xcursor_manager_create(nullptr, 24);
    if (server.cursor_mgr) {
      wlr_xcursor_manager_load(server.cursor_mgr, 1);
    }
    server.cursor = wlr_cursor_create();
    if (server.cursor && server.output_layout) {
      wlr_cursor_attach_output_layout(server.cursor, server.output_layout);
    }
    server.request_set_cursor.notify = [](wl_listener* listener, void* data) {
      auto* server =
        wl_container_of(listener, static_cast<WlrServer*>(nullptr), request_set_cursor);
      auto* event = static_cast<wlr_seat_pointer_request_set_cursor_event*>(data);
      if (!server || !server->cursor || !server->seat) {
        return;
      }
      // Only honor cursor requests from the focused client.
      if (server->seat->pointer_state.focused_client != event->seat_client) {
        return;
      }
      if (event->surface) {
        wlr_cursor_set_surface(
          server->cursor, event->surface, event->hotspot_x, event->hotspot_y);
        log_to_tmp("set_cursor: surface=%p hotspot=%d,%d\n",
                   (void*)event->surface,
                   event->hotspot_x,
                   event->hotspot_y);
      } else {
        log_to_tmp("set_cursor: null surface -> default\n");
        set_default_cursor(server);
      }
      server->pointer_x = server->cursor->x;
      server->pointer_y = server->cursor->y;
      log_pointer_state(server, "set_cursor");
    };
    wl_signal_add(&server.seat->events.request_set_cursor, &server.request_set_cursor);
    // Set an initial default cursor.
    set_default_cursor(&server);
  }
  server.data_device_manager = wlr_data_device_manager_create(server.display);
  if (server.output_layout) {
    server.xdg_output_manager =
      wlr_xdg_output_manager_v1_create(server.display, server.output_layout);
  }

  server.new_xdg_surface.notify = handle_new_xdg_surface;
  wl_signal_add(&server.xdg_shell->events.new_surface, &server.new_xdg_surface);

  server.new_output.notify = handle_new_output;
  wl_signal_add(&server.backend->events.new_output, &server.new_output);
  server.new_input.notify = handle_new_input;
  wl_signal_add(&server.backend->events.new_input, &server.new_input);

  if (!wlr_backend_start(server.backend)) {
    std::fprintf(stderr, "Failed to start wlroots backend\n");
    return EXIT_FAILURE;
  }
  log_to_tmp("startup: pointer init pos (%.1f,%.1f)\n",
             server.pointer_x,
             server.pointer_y);

  const char* socket = wl_display_add_socket_auto(server.display);
  if (!socket) {
    std::fprintf(stderr, "Failed to create Wayland socket\n");
    return EXIT_FAILURE;
  }
  setenv("WAYLAND_DISPLAY", socket, true);
  log_to_tmp("startup: WAYLAND_DISPLAY chosen=%s\n", socket ? socket : "(null)");

  wlr_log(WLR_DEBUG,
          "wlroots compositor ready; WAYLAND_DISPLAY=%s",
          socket ? socket : "(null)");

  wlr_log(WLR_DEBUG,
          "wlroots compositor ready; WAYLAND_DISPLAY=%s",
          socket ? socket : "(null)");

  std::signal(SIGINT, [](int) {
    if (g_server && g_server->display) {
      wl_display_terminate(g_server->display);
    }
  });

  wl_display_run(server.display);

  if (server.engine) {
    server.engine.reset();
  }
  if (server.allocator) {
    wlr_allocator_destroy(server.allocator);
  }
  if (server.renderer) {
    wlr_renderer_destroy(server.renderer);
  }
  if (server.backend) {
    wlr_backend_destroy(server.backend);
  }
  if (server.display) {
    wl_display_destroy(server.display);
  }

  return EXIT_SUCCESS;
}
