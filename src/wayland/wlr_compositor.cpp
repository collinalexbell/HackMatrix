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
#include <wlr/interfaces/wlr_keyboard.h>
#include "interfaces/wlr_input_device.h"
#include <wlr/types/wlr_screencopy_v1.h>
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

#ifdef WLROOTS_DEBUG_LOGS
constexpr bool kWlrootsDebugLogs = true;
#else
constexpr bool kWlrootsDebugLogs = false;
#endif

#if WLROOTS_DEBUG_LOGS
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
#else
static void
log_to_tmp(const char* fmt, ...)
{
  (void)fmt;
}
#endif

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
  wl_listener modifiers;
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

struct ReplayKeyRelease {
  xkb_keysym_t sym = XKB_KEY_NoSymbol;
  uint32_t release_time_msec = 0;
  uint32_t keycode = 0;
  bool update_mods = false;
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
  wlr_screencopy_manager_v1* screencopy_manager = nullptr;
  wlr_output_layout* output_layout = nullptr;
  wlr_xdg_output_manager_v1* xdg_output_manager = nullptr;
  wlr_layer_shell_v1* layer_shell = nullptr;
  wlr_output* primary_output = nullptr;
  wlr_input_device* last_keyboard_device = nullptr;
  wlr_input_device* last_pointer_device = nullptr;
  wlr_keyboard* virtual_keyboard = nullptr;
  wlr_xcursor_manager* cursor_mgr = nullptr;
  wlr_cursor* cursor = nullptr;
  bool cursor_visible = false;
  bool replaySuperActive = false;
  int replaySuperHeld = 0;
  bool pendingReplaySuper = false;
  uint32_t pendingReplaySuperKeycode = 0;
  xkb_keysym_t pendingReplaySuperSym = XKB_KEY_NoSymbol;
  std::vector<ReplayKeyRelease> pendingReplayKeyups;
  std::unordered_map<wlr_surface*, entt::entity> surface_map;
  std::vector<wlr_xdg_surface*> pending_surfaces;
  std::vector<PendingWlAction> pending_wl_actions;
  bool isX11Backend = false;
  double pointer_x = 0.0;
  double pointer_y = 0.0;
  wl_listener request_set_cursor;
};

static void
clear_input_forces(WlrServer* server)
{
  if (!server) {
    return;
  }
  server->input.forward = false;
  server->input.back = false;
  server->input.left = false;
  server->input.right = false;
  server->input.delta_x = 0.0;
  server->input.delta_y = 0.0;
}

static size_t
wayland_app_count(WlrServer* server)
{
  if (!server || !server->registry) {
    return 0;
  }
  size_t count = 0;
  auto view = server->registry->view<WaylandApp::Component>();
  view.each([&](auto /*ent*/, auto& /*comp*/) { ++count; });
  return count;
}

static const wlr_keyboard_impl kVirtualKeyboardImpl = {
  .name = "virtual-keyboard",
  .led_update = nullptr,
};

static wlr_keyboard*
ensure_virtual_keyboard(WlrServer* server)
{
  log_to_tmp("key replay: ensure_virtual_keyboard\n");
  if (!server || !server->seat) {
    log_to_tmp("key replay: virtual keyboard unavailable (missing server/seat)\n");
    return nullptr;
  }
  if (server->virtual_keyboard) {
    return server->virtual_keyboard;
  }
  auto* kbd = new wlr_keyboard();
  wlr_keyboard_init(kbd, &kVirtualKeyboardImpl, "virtual-keyboard");
  xkb_context* ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
  if (ctx) {
    xkb_keymap* keymap =
      xkb_keymap_new_from_names(ctx, nullptr, XKB_KEYMAP_COMPILE_NO_FLAGS);
    if (keymap) {
      wlr_keyboard_set_keymap(kbd, keymap);
      xkb_keymap_unref(keymap);
    } else {
      log_to_tmp("key replay: virtual keyboard keymap creation failed\n");
    }
    xkb_context_unref(ctx);
  } else {
    log_to_tmp("key replay: virtual keyboard xkb context creation failed\n");
  }
  wlr_keyboard_set_repeat_info(kbd, 25, 600);
  wlr_seat_set_keyboard(server->seat, kbd);
  wlr_seat_keyboard_notify_modifiers(server->seat, &kbd->modifiers);
  server->virtual_keyboard = kbd;
  server->last_keyboard_device = &kbd->base;
  log_to_tmp("key replay: installed virtual keyboard for tests\n");
  return kbd;
}

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
  log_to_tmp("pointer[%s]: pos=(%.1f,%.1f) focus=%p visible=%d\n",
             tag ? tag : "",
             px,
             py,
             (void*)surf,
             server->cursor_visible ? 1 : 0);
}

static void
set_default_cursor(WlrServer* server, wlr_output* output = nullptr)
{
  if (!server || !server->cursor || !server->cursor_mgr) {
    log_to_tmp("cursor: default skipped (missing server/cursor/cursor_mgr)\n");
    return;
  }
  float scale = 1.0f;
  if (output) {
    scale = output->scale;
  } else if (server->primary_output) {
    scale = server->primary_output->scale;
  }
  if (scale <= 0.0f) {
    scale = 1.0f;
  }
  // Load the theme for the target scale and set a sane default pointer image.
  int loaded = wlr_xcursor_manager_load(server->cursor_mgr, scale);
  if (loaded < 0) {
    log_to_tmp("cursor: load failed scale=%.2f -> trying Adwaita\n", scale);
    wlr_xcursor_manager_destroy(server->cursor_mgr);
    server->cursor_mgr = wlr_xcursor_manager_create("Adwaita", 24);
    if (server->cursor_mgr) {
      loaded = wlr_xcursor_manager_load(server->cursor_mgr, scale);
    }
  }
  if (loaded < 0 ||
      !wlr_xcursor_manager_get_xcursor(server->cursor_mgr, "left_ptr", scale)) {
    log_to_tmp("cursor: failed theme scale=%.2f -> using software overlay\n", scale);
    return;
  }
  log_to_tmp("cursor: using theme scale=%.2f\n", scale);
  wlr_cursor_set_xcursor(server->cursor, server->cursor_mgr, "left_ptr");
  server->pointer_x = server->cursor->x;
  server->pointer_y = server->cursor->y;
  log_pointer_state(server, "default_cursor");
}

static bool
wayland_pointer_focus_requested(WlrServer* server)
{
  if (!server || !server->engine) {
    return false;
  }
  // If the seat already has a focused pointer surface, keep the cursor visible.
  if (server->seat && server->seat->pointer_state.focused_surface) {
    return true;
  }
  // If the seat has a focused client for the pointer, allow the cursor.
  if (server->seat && server->seat->pointer_state.focused_client) {
    return true;
  }
  auto wm = server->engine->getWindowManager();
  if (!wm || !server->registry) {
    return false;
  }
  auto focused = wm->getCurrentlyFocusedApp();
  if (!focused.has_value() || !server->registry->valid(*focused)) {
    return false;
  }
  return server->registry->all_of<WaylandApp::Component>(*focused);
}

static void
ensure_pointer_focus(WlrServer* server, uint32_t time_msec = 0)
{
  if (!server || !server->seat || server->seat->pointer_state.focused_surface) {
    return;
  }
  if (!server->engine || !server->registry) {
    return;
  }
  auto wm = server->engine->getWindowManager();
  if (!wm) {
    return;
  }
  auto focused = wm->getCurrentlyFocusedApp();
  if (!focused || !server->registry->valid(*focused) ||
      !server->registry->all_of<WaylandApp::Component>(*focused)) {
    return;
  }
  auto& comp = server->registry->get<WaylandApp::Component>(*focused);
  auto* app = comp.app.get();
  wlr_surface* surf = app ? app->getSurface() : nullptr;
  if (!surf) {
    return;
  }
  if (time_msec == 0) {
    uint64_t now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now().time_since_epoch())
                        .count();
    time_msec = static_cast<uint32_t>(now_ms & 0xffffffff);
  }
  wlr_seat_pointer_notify_enter(
    server->seat, surf, server->pointer_x, server->pointer_y);
  wlr_seat_pointer_notify_motion(
    server->seat, time_msec, server->pointer_x, server->pointer_y);
  wlr_seat_pointer_notify_frame(server->seat);
}

static void
set_cursor_visible(WlrServer* server, bool visible, wlr_output* output = nullptr)
{
  if (!server || !server->cursor) {
    return;
  }
  log_to_tmp("cursor: set_cursor_visible visible=%d current=%d\n",
             visible ? 1 : 0,
             server->cursor_visible ? 1 : 0);
  if (visible == server->cursor_visible) {
    return;
  }
  if (visible) {
    // Always ensure a sane default image; clients can override via set_cursor.
    set_default_cursor(server, output);
  } else {
    wlr_cursor_unset_image(server->cursor);
    if (server->seat) {
      wlr_seat_pointer_notify_clear_focus(server->seat);
    }
  }
  server->cursor_visible = visible;
}

static bool
isSuperSym(xkb_keysym_t sym)
{
  return sym == XKB_KEY_Super_L || sym == XKB_KEY_Super_R;
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
  FILE* f = nullptr;
  // Detect cases where surfaces exist but no Wayland apps are registered and
  // requeue adds so they don't stall.
  if (server->surface_map.size() > 0) {
    auto view = server->registry->view<WaylandApp::Component>();
    if (view.size() == 0) {
      for (auto& entry : server->surface_map) {
        auto it = server->surface_map.find(entry.first);
        if (it != server->surface_map.end()) {
          entt::entity ent = it->second;
          if (server->registry->valid(ent)) {
            if (auto* comp = server->registry->try_get<WaylandApp::Component>(ent)) {
              if (comp->app) {
                server->pending_wl_actions.push_back(
                  PendingWlAction{ PendingWlAction::Add, comp->app, entry.first });
              }
            }
          }
        }
      }
    }
  }
#if WLROOTS_DEBUG_LOGS
  f = std::fopen("/tmp/matrix-wlroots-output.log", "a");
  if (!server->pending_wl_actions.empty() && f) {
    std::fprintf(f,
                 "wayland deferred queue drain: count=%zu\n",
                 server->pending_wl_actions.size());
    std::fflush(f);
  }
#endif
  auto actions = std::move(server->pending_wl_actions);
  server->pending_wl_actions.clear();
  for (auto& action : actions) {
    if (action.type == PendingWlAction::Add) {
      // Drop duplicate adds for the same surface if it's already registered.
      if (server->surface_map.find(action.surface) != server->surface_map.end()) {
        if (f) {
          std::fprintf(f,
                       "wayland app add skipped (already registered): surface=%p\n",
                       (void*)action.surface);
          std::fflush(f);
        }
        continue;
      }
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
        auto* comp = server->registry->try_get<WaylandApp::Component>(entity);
        // Request a default window size that matches the X11 defaults.
        if (comp && comp->app) {
          comp->app->requestSize(Bootable::DEFAULT_WIDTH, Bootable::DEFAULT_HEIGHT);
        }
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
        log_to_tmp("wayland app add (deferred): surface=%p ent=%d texId=%d texUnit=%d app=%p\n",
                   (void*)action.surface,
                   (int)entt::to_integral(entity),
                   action.app->getTextureId(),
                   action.app->getTextureUnit() - GL_TEXTURE0,
                   (void*)action.app.get());
        if (server->engine) {
          if (auto api = server->engine->getApi()) {
            api->forceUpdateCachedStatus();
          }
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
          if (auto wm = server->engine ? server->engine->getWindowManager() : nullptr) {
            if (auto focused = wm->getCurrentlyFocusedApp(); focused && *focused == e) {
              wm->unfocusApp();
            }
          }
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
      if (server->engine) {
        if (auto api = server->engine->getApi()) {
          api->forceUpdateCachedStatus();
        }
      }
      if (f) {
        std::fprintf(f,
                     "wayland app remove (deferred): surface=%p\n",
                     (void*)action.surface);
        std::fflush(f);
      }
      log_to_tmp("wayland app remove (deferred): surface=%p\n", (void*)action.surface);
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
  bool unmapLinked = false;
};

static void
safe_remove_listener(wl_listener* listener)
{
  if (!listener) {
    return;
  }
  if (listener->link.prev || listener->link.next) {
    wl_list_remove(&listener->link);
    listener->link.prev = nullptr;
    listener->link.next = nullptr;
  }
}

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
  wl_list_remove(&handle->modifiers.link);
  wl_list_remove(&handle->key.link);
  wl_list_remove(&handle->destroy.link);
  delete handle;
}

struct KeycodeLookupResult {
  xkb_keycode_t keycode = 0;
  bool needsShift = false;
};

static std::optional<KeycodeLookupResult>
keycode_for_keysym(wlr_keyboard* keyboard, xkb_keysym_t sym)
{
  if (!keyboard || !keyboard->keymap) {
    return std::nullopt;
  }
  const xkb_keycode_t min = xkb_keymap_min_keycode(keyboard->keymap);
  const xkb_keycode_t max = xkb_keymap_max_keycode(keyboard->keymap);
  for (xkb_keycode_t code = min; code <= max; ++code) {
    int layouts = xkb_keymap_num_layouts_for_key(keyboard->keymap, code);
    for (int layout = 0; layout < layouts; ++layout) {
      int levels = xkb_keymap_num_levels_for_key(keyboard->keymap, code, layout);
      for (int level = 0; level < levels; ++level) {
        const xkb_keysym_t* syms = nullptr;
        int nsyms =
          xkb_keymap_key_get_syms_by_level(keyboard->keymap, code, layout, level, &syms);
        for (int i = 0; i < nsyms; ++i) {
          if (syms[i] == sym) {
            KeycodeLookupResult res;
            res.keycode = code;
            res.needsShift = level > 0;
            return res;
          }
        }
      }
    }
  }
  return std::nullopt;
}

static void
process_key_sym(WlrServer* server,
                wlr_keyboard* keyboard,
                xkb_keysym_t sym,
                bool pressed,
                uint32_t time_msec,
                uint32_t keycode = 0,
                bool update_mods = false)
{
  if (!server) {
    return;
  }

  if (isSuperSym(sym)) {
    if (pressed) {
      ++server->replaySuperHeld;
    } else if (server->replaySuperHeld > 0) {
      --server->replaySuperHeld;
    }
    server->replaySuperActive = server->replaySuperHeld > 0;
    log_to_tmp("super combo: super key state pressed=%d replaySuper=%d time=%u keycode=%u\n",
               pressed ? 1 : 0,
               server->replaySuperActive ? 1 : 0,
               time_msec,
               keycode);
  }
  if (update_mods && keyboard) {
    uint32_t depressed = keyboard->modifiers.depressed;
    if (sym == XKB_KEY_Shift_L || sym == XKB_KEY_Shift_R) {
      if (pressed) {
        depressed |= WLR_MODIFIER_SHIFT;
      } else {
        depressed &= ~WLR_MODIFIER_SHIFT;
      }
    }
    if (isSuperSym(sym)) {
      if (pressed) {
        depressed |= WLR_MODIFIER_LOGO;
      } else {
        // Only clear LOGO when no other Super is logically held.
        if (server->replaySuperHeld <= 0) {
          depressed &= ~WLR_MODIFIER_LOGO;
        }
      }
    }
    keyboard->modifiers.depressed = depressed;
    wlr_keyboard_notify_modifiers(keyboard,
                                  keyboard->modifiers.depressed,
                                  keyboard->modifiers.latched,
                                  keyboard->modifiers.locked,
                                  keyboard->modifiers.group);
    if (server->seat) {
      wlr_seat_keyboard_notify_modifiers(server->seat, &keyboard->modifiers);
    }
  }

  // Detect if a Wayland app currently has WM focus; if so, avoid feeding
  // movement/game controls so keys pass through to the client.
  bool waylandFocusActive = false;
  bool blockClientDelivery = false;
  if (server->engine) {
    if (auto wm = server->engine->getWindowManager()) {
      if (auto focused = wm->getCurrentlyFocusedApp()) {
        if (server->registry &&
            server->registry->all_of<WaylandApp::Component>(*focused)) {
          waylandFocusActive = true;
        }
      }
    }
  }

  if (sym == XKB_KEY_Escape && pressed && !waylandFocusActive) {
    wl_display_terminate(server->display);
  }
  if (pressed && server->engine) {
    if (auto wm = server->engine->getWindowManager()) {
      uint32_t mods = keyboard ? wlr_keyboard_get_modifiers(keyboard) : 0;
      bool superHeld = (mods & WLR_MODIFIER_LOGO) || server->replaySuperActive;
      bool shiftHeld = mods & WLR_MODIFIER_SHIFT;
      bool consumedHotkey = false;
      if (pressed && (sym == XKB_KEY_e || sym == XKB_KEY_E)) {
        log_to_tmp("super combo: state pressed mods=0x%x replaySuper=%d superHeld=%d waylandFocus=%d time=%u keycode=%u\n",
                   mods,
                   server->replaySuperActive ? 1 : 0,
                   superHeld ? 1 : 0,
                   waylandFocusActive ? 1 : 0,
                   time_msec,
                   keycode);
      }
      if (superHeld && (sym == XKB_KEY_E || sym == XKB_KEY_e)) {
        entt::entity focusedEnt{};
        bool haveFocus = false;
        if (auto focused = wm->getCurrentlyFocusedApp()) {
          focusedEnt = *focused;
          haveFocus = true;
        }
        size_t wlCountBefore = wayland_app_count(server);
        log_to_tmp("super combo: about to unfocus (pressed=%d mods=0x%x replaySuper=%d time=%u keycode=%u focused=%d wlCount=%zu)\n",
                   pressed ? 1 : 0,
                   mods,
                   server->replaySuperActive ? 1 : 0,
                   time_msec,
                   keycode,
                   haveFocus ? (int)entt::to_integral(focusedEnt) : -1,
                   wlCountBefore);
        log_to_tmp("super combo: triggered -> unfocus (pressed=%d mods=0x%x replaySuper=%d time=%u keycode=%u)\n",
                   pressed ? 1 : 0,
                   mods,
                   server->replaySuperActive ? 1 : 0,
                   time_msec,
                   keycode);
        wm->unfocusApp();
        waylandFocusActive = false;
        if (server->seat) {
          wlr_seat_keyboard_notify_clear_focus(server->seat);
          wlr_seat_pointer_notify_clear_focus(server->seat);
        }
        size_t wlCountAfter = wayland_app_count(server);
        log_to_tmp("super combo: unfocus done focused=%d wlCount=%zu\n",
                   haveFocus ? (int)entt::to_integral(focusedEnt) : -1,
                   wlCountAfter);
        consumedHotkey = true;
        server->replaySuperActive = false;
      }
      if (superHeld &&
          (sym >= XKB_KEY_1 && sym <= XKB_KEY_9)) {
        int idx = static_cast<int>(sym - XKB_KEY_1);
        log_to_tmp("super hotkey: idx=%d mods=0x%x time=%u keycode=%u\n",
                   idx,
                   mods,
                   time_msec,
                   keycode);
        wm->handleHotkeySym(sym, superHeld, shiftHeld);
        consumedHotkey = true;
        return;
      }
      if (!consumedHotkey) {
        if (superHeld) {
          wm->handleHotkeySym(sym, superHeld, shiftHeld);
          consumedHotkey = true; // Treat Super combos as compositor-level.
        } else if (!waylandFocusActive) {
          wm->handleHotkeySym(sym, superHeld, shiftHeld);
        }
      }
      if (consumedHotkey) {
        return;
      }
    }
  }
  switch (sym) {
    case XKB_KEY_w:
    case XKB_KEY_W:
      if (!waylandFocusActive) {
        server->input.forward = pressed;
      }
      break;
    case XKB_KEY_s:
    case XKB_KEY_S:
      if (!waylandFocusActive) {
        server->input.back = pressed;
      }
      break;
    case XKB_KEY_a:
    case XKB_KEY_A:
      if (!waylandFocusActive) {
        server->input.left = pressed;
      }
      break;
    case XKB_KEY_d:
    case XKB_KEY_D:
      if (!waylandFocusActive) {
        server->input.right = pressed;
      }
      break;
    case XKB_KEY_r:
    case XKB_KEY_R:
      if (pressed && server->engine && !waylandFocusActive) {
        if (auto wm = server->engine->getWindowManager()) {
          log_to_tmp("focus hotkey: applying focusLookedAtApp\n");
          wm->focusLookedAtApp();
          clear_input_forces(server);
          blockClientDelivery = true; // consume focus hotkey
        }
      }
      break;
    case XKB_KEY_v:
    case XKB_KEY_V:
      if (pressed && server->engine && !waylandFocusActive) {
        if (auto wm = server->engine->getWindowManager()) {
          log_to_tmp("menu hotkey pressed (V)\n");
          wm->menu();
        }
      }
      break;
    default:
      break;
  }
  if constexpr (kWlrootsDebugLogs) {
    FILE* f = std::fopen("/tmp/matrix-wlroots-wm.log", "a");
    if (f) {
      std::fprintf(f, "key sym=%u pressed=%d\n", sym, pressed ? 1 : 0);
      std::fflush(f);
      std::fclose(f);
    }
    wlr_log(WLR_DEBUG, "key sym=%u pressed=%d", sym, pressed ? 1 : 0);
  }

  if (blockClientDelivery) {
    return;
  }
  if (server->seat && keyboard && time_msec != 0) {
    // Deliver to the seat-focused surface if it maps to a Wayland app; otherwise fall
    // back to the WM-focused Wayland app.
    wlr_surface* target_surface = server->seat->keyboard_state.focused_surface;
    if (!target_surface && server->seat && server->seat->pointer_state.focused_surface) {
      target_surface = server->seat->pointer_state.focused_surface;
    }
      if (!target_surface && server->engine) {
        if (auto wm = server->engine->getWindowManager()) {
          if (auto focused = wm->getCurrentlyFocusedApp()) {
            if (server->registry &&
                server->registry->all_of<WaylandApp::Component>(*focused)) {
              auto& comp = server->registry->get<WaylandApp::Component>(*focused);
              target_surface = comp.app ? comp.app->getSurface() : nullptr;
            }
          }
        }
      }
      if (target_surface) {
        // Keep WM focus in sync with the seat when we can map the surface.
        if (server->engine) {
          if (auto wm = server->engine->getWindowManager()) {
            auto it = server->surface_map.find(target_surface);
            if (it != server->surface_map.end()) {
              // Do not force WM focus here; explicit hotkeys handle focus.
            }
          }
        }
        wlr_seat_set_keyboard(server->seat, keyboard);
        if (server->seat->keyboard_state.focused_surface != target_surface) {
          wlr_seat_keyboard_notify_enter(server->seat,
                                         target_surface,
                                         keyboard->keycodes,
                                         keyboard->num_keycodes,
                                         &keyboard->modifiers);
          wlr_seat_keyboard_notify_modifiers(server->seat, &keyboard->modifiers);
        }
      enum wl_keyboard_key_state state =
        pressed ? WL_KEYBOARD_KEY_STATE_PRESSED : WL_KEYBOARD_KEY_STATE_RELEASED;
      wlr_seat_keyboard_notify_key(server->seat, time_msec, keycode, state);
      wlr_seat_keyboard_notify_modifiers(server->seat, &keyboard->modifiers);
    }
  }
}

static void
flush_pending_replay_keyups(WlrServer* server,
                            wlr_keyboard* keyboard,
                            uint32_t now_msec)
{
  if (!server || server->pendingReplayKeyups.empty()) {
    return;
  }
  auto it = server->pendingReplayKeyups.begin();
  while (it != server->pendingReplayKeyups.end()) {
    if (now_msec >= it->release_time_msec) {
      process_key_sym(server,
                      keyboard,
                      it->sym,
                      false,
                      now_msec,
                      it->keycode,
                      it->update_mods);
      it = server->pendingReplayKeyups.erase(it);
    } else {
      ++it;
    }
  }
}

void
handle_keyboard_key(wl_listener* listener, void* data)
{
  auto* handle =
    wl_container_of(listener, static_cast<WlrKeyboardHandle*>(nullptr), key);
  auto* server = handle->server;
  auto* event = static_cast<wlr_keyboard_key_event*>(data);
  // wlroots key events carry a hardware/libinput keycode. Add 8 only for xkb
  // lookup; keep the raw code for notifying the seat.
  uint32_t xkb_keycode = event->keycode + 8;
  const xkb_keysym_t* syms;
  int nsyms =
    xkb_state_key_get_syms(handle->keyboard->xkb_state, xkb_keycode, &syms);
  for (int i = 0; i < nsyms; ++i) {
    bool pressed = event->state == WL_KEYBOARD_KEY_STATE_PRESSED;
    process_key_sym(
      server, handle->keyboard, syms[i], pressed, event->time_msec, event->keycode);
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
        // Only show the cursor when a Wayland app is focused.
        set_cursor_visible(server, wayland_pointer_focus_requested(server));
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
        ensure_pointer_focus(handle->server, event->time_msec);
        if (handle->server->seat && wayland_pointer_focus_requested(handle->server)) {
          wlr_seat_pointer_notify_motion(handle->server->seat,
                                         event->time_msec,
                                         handle->server->pointer_x,
                                         handle->server->pointer_y);
          wlr_seat_pointer_notify_frame(handle->server->seat);
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
        ensure_pointer_focus(handle->server, event->time_msec);
        if (handle->server->seat && wayland_pointer_focus_requested(handle->server)) {
          wlr_seat_pointer_notify_motion(handle->server->seat,
                                         event->time_msec,
                                         handle->server->pointer_x,
                                         handle->server->pointer_y);
          wlr_seat_pointer_notify_frame(handle->server->seat);
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
            // Prefer focusing the surface under the pointer to keep WM focus in sync.
            wlr_surface* pointer_surface = nullptr;
            if (handle->server->seat && handle->server->seat->pointer_state.focused_surface) {
              pointer_surface = handle->server->seat->pointer_state.focused_surface;
            }
            bool focusedViaPointer = false;
            if (pointer_surface) {
              auto it = handle->server->surface_map.find(pointer_surface);
              if (it != handle->server->surface_map.end()) {
                entt::entity ent = it->second;
                wm->focusApp(ent);
                if (auto* comp =
                      handle->server->registry->try_get<WaylandApp::Component>(ent)) {
                  if (comp->app) {
                    comp->app->takeInputFocus();
                  }
                }
                focusedViaPointer = true;
              }
            }
            if (!focusedViaPointer) {
              wm->focusLookedAtApp();
            }
          }
        }
        if (handle->server->seat) {
          ensure_pointer_focus(handle->server, event->time_msec);
          wlr_seat_pointer_notify_button(handle->server->seat,
                                         event->time_msec,
                                         event->button,
                                         event->state);
          wlr_seat_pointer_notify_frame(handle->server->seat);
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
  handle->modifiers.notify = [](wl_listener* listener, void* data) {
    (void)data;
    auto* handle =
      wl_container_of(listener, static_cast<WlrKeyboardHandle*>(nullptr), modifiers);
    if (handle->server && handle->server->seat) {
      wlr_seat_keyboard_notify_modifiers(handle->server->seat,
                                         &handle->keyboard->modifiers);
    }
  };
  wl_signal_add(&keyboard->events.modifiers, &handle->modifiers);
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
    if (!handle->server || !surf || handle->registered) {
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
    handle->registered = true;
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
      handle->registered = false;
      log_to_tmp("wayland deferred remove queued (unmap): surface=%p app=%p\n",
                 (void*)handle->surface,
                 (void*)handle->app.get());
      ensure_wayland_apps_registered(handle->server);
    }
    safe_remove_listener(&handle->unmap);
    handle->unmapLinked = false;
  };
  wl_signal_add(&xdg_surface->surface->events.unmap, &handle->unmap);
  handle->unmapLinked = true;
  handle->destroy.notify = [](wl_listener* listener, void* data) {
    auto* handle = wl_container_of(listener, static_cast<WaylandAppHandle*>(nullptr), destroy);
    auto* surf = static_cast<wlr_surface*>(data);
    if (handle->server) {
      handle->server->pending_wl_actions.push_back(
        PendingWlAction{ PendingWlAction::Remove, handle->app, surf });
      handle->registered = false;
      log_to_tmp("wayland deferred remove queued (destroy): surface=%p app=%p\n",
                 (void*)surf,
                 (void*)handle->app.get());
      ensure_wayland_apps_registered(handle->server);
    }
    safe_remove_listener(&handle->unmap);
    handle->unmapLinked = false;
    safe_remove_listener(&handle->destroy);
    safe_remove_listener(&handle->commit);
    delete handle;
  };
  wl_signal_add(&xdg_surface->surface->events.destroy, &handle->destroy);

  if (server->primary_output) {
    wlr_surface_send_enter(xdg_surface->surface, server->primary_output);
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
    log_to_tmp("xdg_surface mapped surface=%p role=%d\n",
               (void*)handle->xdg->surface,
               handle->xdg->role);
    if (!handle->created) {
      handle->created = true;
      create_wayland_app(handle->server, handle->xdg);
    }
  };
  wl_signal_add(&xdg_surface->surface->events.map, &handle->map);

  handle->destroy.notify = [](wl_listener* listener, void* data) {
    auto* handle = wl_container_of(listener, static_cast<XdgSurfaceHandle*>(nullptr), destroy);
    auto* xdg = static_cast<wlr_xdg_surface*>(data);
    log_to_tmp("xdg_surface destroy: root=%p surface=%p role=%d\n",
               (void*)xdg,
               xdg ? (void*)xdg->surface : nullptr,
               xdg ? (int)xdg->role : -1);
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
  // Reduce log spam in hot path; enable by setting WLROOTS_FRAME_LOG=1.
  if (std::getenv("WLROOTS_FRAME_LOG")) {
    log_to_tmp("frame: swapchain %dx%d output %s\n",
               width,
               height,
               handle->output ? handle->output->name : "(null)");
  }

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
  // Surface API quit handling through wl_display so tests can terminate cleanly.
  if (server->engine) {
    auto api = server->engine->getApi();
    if (api) {
      api->setDisplay(server->display);
    }
  }
  }
  if (server->engine && !server->registry) {
    server->registry = server->engine->getRegistry();
  }
  // Ensure Wayland apps have textures attached once the renderer exists.
  ensure_wayland_apps_registered(server);

  if (server->engine) {
    auto wm = server->engine->getWindowManager();
    if (wm) {
      uint64_t now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                          std::chrono::steady_clock::now().time_since_epoch())
                          .count();
      uint32_t now_msec = static_cast<uint32_t>(now_ms & 0xffffffff);
      auto ready = wm->consumeReadyReplaySyms(now_ms);
      wlr_keyboard* kbd = nullptr;
      if (server->last_keyboard_device) {
        kbd = wlr_keyboard_from_input_device(server->last_keyboard_device);
      } else if (server->seat && server->seat->keyboard_state.keyboard) {
        kbd = server->seat->keyboard_state.keyboard;
      } else {
        kbd = ensure_virtual_keyboard(server);
      }
      flush_pending_replay_keyups(server, kbd, now_msec);
      static bool logged_missing_keyboard = false;
      if (!kbd && !logged_missing_keyboard) {
        log_to_tmp("key replay: no keyboard device available for injection\n");
        logged_missing_keyboard = true;
      }
      auto shift_lookup = keycode_for_keysym(kbd, XKB_KEY_Shift_L);
      auto super_lookup = keycode_for_keysym(kbd, XKB_KEY_Super_L);
      uint32_t base_msec = now_msec;
      uint32_t step = 0;
      constexpr uint32_t kReplayHoldMs = 16;
      for (size_t i = 0; i < ready.size(); ++i) {
        auto sym = ready[i].sym;
        auto lookup = keycode_for_keysym(kbd, sym);
        if (!lookup.has_value() || lookup->keycode == 0) {
          log_to_tmp("key replay: no keycode for sym=%u\n", sym);
          continue;
        }
        uint32_t hw_keycode = lookup->keycode >= 8 ? lookup->keycode - 8 : lookup->keycode;
        uint32_t time_msec = base_msec + step;
        log_to_tmp("key replay: sym=%u keycode=%u hw=%u time=%u\n",
                   sym,
                   lookup->keycode,
                   hw_keycode,
                   time_msec);
        bool is_super_sym = (sym == XKB_KEY_Super_L || sym == XKB_KEY_Super_R);
        bool is_last = (i + 1) == ready.size();
        bool replay_has_future = wm->hasPendingReplay();
        if (is_super_sym && server->pendingReplaySuper &&
            server->pendingReplaySuperKeycode != 0) {
          // Avoid overlapping super presses that clear modifiers out of order.
          process_key_sym(server,
                          kbd,
                          server->pendingReplaySuperSym,
                          false,
                          time_msec,
                          server->pendingReplaySuperKeycode,
                          true);
          server->pendingReplaySuper = false;
          server->pendingReplaySuperKeycode = 0;
          server->pendingReplaySuperSym = XKB_KEY_NoSymbol;
        }
        if (is_super_sym && super_lookup.has_value() &&
            (replay_has_future || !is_last)) {
          server->pendingReplaySuper = true;
          server->pendingReplaySuperKeycode = hw_keycode;
          server->pendingReplaySuperSym = sym;
          process_key_sym(server, kbd, sym, true, time_msec, hw_keycode, true);
          // Do not release yet; move to next symbol.
          step += 1;
          continue;
        }
        if (lookup->needsShift && shift_lookup.has_value() && shift_lookup->keycode) {
          uint32_t shift_hw = shift_lookup->keycode >= 8 ? shift_lookup->keycode - 8
                                                         : shift_lookup->keycode;
          process_key_sym(server,
                          kbd,
                          XKB_KEY_Shift_L,
                          true,
                          time_msec,
                          shift_hw,
                          true);
          time_msec += 1;
          step += 1;
        }
        process_key_sym(server, kbd, sym, true, time_msec, hw_keycode, true);
        uint64_t hold_ms = kReplayHoldMs;
        if (i + 1 < ready.size()) {
          auto delta_ms = ready[i + 1].ready_ms - ready[i].ready_ms;
          if (delta_ms > hold_ms) {
            hold_ms = static_cast<uint32_t>(std::min<uint64_t>(delta_ms, 10000));
          }
        }
        uint32_t release_time = time_msec + static_cast<uint32_t>(hold_ms);
        server->pendingReplayKeyups.push_back(
          ReplayKeyRelease{ sym, release_time, hw_keycode, true });
        if (lookup->needsShift && shift_lookup.has_value() && shift_lookup->keycode) {
          uint32_t shift_hw = shift_lookup->keycode >= 8 ? shift_lookup->keycode - 8
                                                         : shift_lookup->keycode;
          server->pendingReplayKeyups.push_back(
            ReplayKeyRelease{ XKB_KEY_Shift_L, release_time + 1, shift_hw, true });
          step += 1;
        }
        step += 2;
        if (server->pendingReplaySuper && !is_super_sym) {
          // Release held super after the combo key with a realistic hold so the
          // replay path mirrors physical key timing.
          uint32_t super_release_time =
            time_msec + static_cast<uint32_t>(std::max<uint64_t>(hold_ms, kReplayHoldMs));
          server->pendingReplayKeyups.push_back(
            ReplayKeyRelease{ server->pendingReplaySuperSym,
                              super_release_time,
                              server->pendingReplaySuperKeycode,
                              true });
          server->pendingReplaySuper = false;
          server->pendingReplaySuperKeycode = 0;
          server->pendingReplaySuperSym = XKB_KEY_NoSymbol;
          step += 1;
        }
      }
    }
  }

  if (server->engine) {
    Camera* camera = server->engine->getCamera();
    if (camera) {
      camera->handleTranslateForce(
        server->input.forward, server->input.back, server->input.left, server->input.right);
      bool pointerFocusRequested = wayland_pointer_focus_requested(server);
      if (!pointerFocusRequested &&
          (server->input.delta_x != 0.0 || server->input.delta_y != 0.0)) {
        camera->handleRotateForce(
          nullptr, server->input.delta_x, -server->input.delta_y);
      }
      // Hide cursor and clear pointer focus when game mode is active so only deltas matter.
      if (!pointerFocusRequested && server->seat &&
          server->seat->pointer_state.focused_surface) {
        wlr_seat_pointer_notify_clear_focus(server->seat);
      }
      set_cursor_visible(server, pointerFocusRequested, handle->output);
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
  if (std::getenv("WLROOTS_FRAME_LOG")) {
    log_to_tmp("frame: glViewport %dx%d\n", width, height);
  }
  double frameStart = currentTimeSeconds();
  server->engine->frame(frameStart);
  if (server->engine) {
    if (auto wm = server->engine->getWindowManager()) {
      if (wm->consumeScreenshotRequest()) {
        if (auto renderer = server->engine->getRenderer()) {
          renderer->screenshotFromCurrentFramebuffer(width, height);
        }
      }
    }
  }

  // Render software cursors (clients set them via wl_pointer.set_cursor).
  pixman_region32_t cursor_damage;
  pixman_region32_init_rect(&cursor_damage, 0, 0, width, height);
  wlr_output_add_software_cursors_to_render_pass(handle->output, pass, &cursor_damage);
  pixman_region32_fini(&cursor_damage);
  // Draw a compositor-owned software cursor as a fallback when focused.
  if (server->engine && wayland_pointer_focus_requested(server)) {
    if (auto* renderer = server->engine->getRenderer()) {
      float sizePx = 24.0f * (handle->output ? handle->output->scale : 1.0f);
      renderer->renderSoftwareCursor(server->pointer_x, server->pointer_y, sizePx);
    }
  }
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
      set_cursor_visible(server, wayland_pointer_focus_requested(server), output);
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

} // namespace

int
main(int argc, char** argv, char** envp)
{
  (void)argc;
  (void)argv;
  WlrServer server = {};
  server.envp = envp;

  wlr_log_init(WLR_DEBUG, nullptr);
  // Write PID for test harness so it can kill the compositor reliably.
  {
    const char* pidPath = "/tmp/matrix-wlroots.pid";
    FILE* f = std::fopen(pidPath, "w");
    if (f) {
      std::fprintf(f, "%d\n", (int)getpid());
      std::fclose(f);
      log_to_tmp("startup: wrote pid file %s\n", pidPath);
    }
  }
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
  const char* backend_kind = "unknown";
  if (server.isX11Backend) {
    backend_kind = "x11";
  } else if (std::getenv("WAYLAND_DISPLAY")) {
    backend_kind = "wayland";
  }
  log_to_tmp("startup: backend kind=%s env=%s\n",
             backend_kind,
             backend_env ? backend_env : "(null)");

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
    // Prefer the default theme, but allow fallback later if it is missing.
    server.cursor_mgr = wlr_xcursor_manager_create("default", 24);
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
        server->cursor_visible = true;
        log_to_tmp("set_cursor: surface=%p hotspot=%d,%d\n",
                   (void*)event->surface,
                   event->hotspot_x,
                   event->hotspot_y);
      } else {
        log_to_tmp("set_cursor: null surface -> default\n");
        set_cursor_visible(server, wayland_pointer_focus_requested(server));
      }
      server->pointer_x = server->cursor->x;
      server->pointer_y = server->cursor->y;
      log_pointer_state(server, "set_cursor");
    };
    wl_signal_add(&server.seat->events.request_set_cursor, &server.request_set_cursor);
    // Set an initial default cursor.
    set_cursor_visible(&server, wayland_pointer_focus_requested(&server));
  }
  server.data_device_manager = wlr_data_device_manager_create(server.display);
  // Advertise the screencopy protocol so tools like grim can capture frames.
  server.screencopy_manager = wlr_screencopy_manager_v1_create(server.display);
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
  if (server.seat) {
    wl_list_remove(&server.request_set_cursor.link);
  }
  if (server.virtual_keyboard) {
    wlr_keyboard_finish(server.virtual_keyboard);
    delete server.virtual_keyboard;
    server.virtual_keyboard = nullptr;
  }
  wl_list_remove(&server.new_input.link);
  wl_list_remove(&server.new_output.link);
  wl_list_remove(&server.new_xdg_surface.link);
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
