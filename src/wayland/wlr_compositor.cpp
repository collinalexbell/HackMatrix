#define WLR_USE_UNSTABLE 1

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <algorithm>
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
#include <linux/input-event-codes.h>
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
#include <cctype>
#include <thread>

static std::string
keysym_name(xkb_keysym_t sym)
{
  char buf[64] = {0};
  xkb_keysym_get_name(sym, buf, sizeof(buf));
  return std::string(buf);
}

#include "engine.h"
#include "wayland_app.h"
#include "AppSurface.h"
#include "entity.h"
#include "screen.h"
#include "Config.h"
#include "controls.h"
#include "wayland/wlr_compositor.h"

#ifdef WLROOTS_DEBUG_LOGS
constexpr bool kWlrootsDebugLogs = true;
#else
constexpr bool kWlrootsDebugLogs = false;
#endif

static bool
wlroots_debug_logs_enabled()
{
  // Enable only if set to 1
  if (const char* env = std::getenv("WLROOTS_DEBUG_LOGS")) {
    return std::strcmp(env, "1") == 0;
  }
  return false;
}

namespace {

static bool
is_hotkey_sym(HotkeyModifier mod, xkb_keysym_t sym)
{
  switch (mod) {
    case HotkeyModifier::Alt:
      return sym == XKB_KEY_Alt_L || sym == XKB_KEY_Alt_R;
    case HotkeyModifier::Super:
    default:
      return sym == XKB_KEY_Super_L || sym == XKB_KEY_Super_R;
  }
}

double
currentTimeSeconds()
{
  static const auto start = std::chrono::steady_clock::now();
  const auto now = std::chrono::steady_clock::now();
  std::chrono::duration<double> elapsed = now - start;
  return elapsed.count();
}

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
  if (!server || !server->seat) {
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
    }
    xkb_context_unref(ctx);
  }
  wlr_keyboard_set_repeat_info(kbd, 25, 600);
  wlr_seat_set_keyboard(server->seat, kbd);
  wlr_seat_keyboard_notify_modifiers(server->seat, &kbd->modifiers);
  server->virtual_keyboard = kbd;
  server->last_keyboard_device = &kbd->base;
  return kbd;
}

static void
set_default_cursor(WlrServer* server, wlr_output* output = nullptr)
{
  if (!server || !server->cursor || !server->cursor_mgr) {
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
    wlr_xcursor_manager_destroy(server->cursor_mgr);
    server->cursor_mgr = wlr_xcursor_manager_create("Adwaita", 24);
    if (server->cursor_mgr) {
      loaded = wlr_xcursor_manager_load(server->cursor_mgr, scale);
    }
  }
  if (loaded < 0 ||
      !wlr_xcursor_manager_get_xcursor(server->cursor_mgr, "left_ptr", scale)) {
    return;
  }
  wlr_cursor_set_xcursor(server->cursor, server->cursor_mgr, "left_ptr");
  server->pointer_x = server->cursor->x;
  server->pointer_y = server->cursor->y;
}

bool isValidWaylandAppComponent(WlrServer* server, entt::entity entity) {
	return server->registry->valid(entity) &&
		server->registry->all_of<WaylandApp::Component>(entity);
}

// Map the global pointer into surface-local coords assuming the surface is
// centered in the output (matches render overlay) and clamp to surface size.
//
// TODO: Remove the clamp which is causing inputs that aren't on the window to be passed into the window.
static std::pair<double, double>
map_pointer_to_surface(WlrServer* server, wlr_surface* surf)
{
  double sx = server ? server->pointer_x : 0.0;
  double sy = server ? server->pointer_y : 0.0;
  double surf_w = surf ? surf->current.width : 0;
  double surf_h = surf ? surf->current.height : 0;

  bool handledPopupCoords = false;
  // For popup/accessory surfaces, place the pointer relative to the parent's
  // top-left using the same math as renderWaylandPopup().
  if (server && surf && server->registry) {
    auto it = server->surface_map.find(surf);
    if (it != server->surface_map.end() && isValidWaylandAppComponent(server, it->second)) {
      auto& comp = server->registry->get<WaylandApp::Component>(it->second);
      if (comp.accessory) {
        auto& parentComp = server->registry->get<WaylandApp::Component>(comp.parent);
        if (parentComp.app) {
          int parentW = parentComp.app->getWidth();
          int parentH = parentComp.app->getHeight();
          double out_w = server->primary_output->width;
          double out_h = server->primary_output->height;
          double parentLeft = std::max(0.0, (out_w - parentW) * 0.5);
          double parentTop = std::max(0.0, (out_h - parentH) * 0.5);
          double popupLeft = parentLeft + comp.offset_x;
          double popupTop = parentTop + comp.offset_y;
          sx -= popupLeft;
          sy -= popupTop;
          handledPopupCoords = true;
        }
      }
    }
  }
  // Subtract the center offset so (0,0) maps to the surface top-left.
  // Skip this for popups, which already derived screen-relative coords.
  if (!handledPopupCoords && server && server->primary_output) {
    double out_w = server->primary_output->width;
    double out_h = server->primary_output->height;
    if (out_w > surf_w && surf_w > 0) {
      sx -= (out_w - surf_w) / 2.0;
    }
    if (out_h > surf_h && surf_h > 0) {
      sy -= (out_h - surf_h) / 2.0;
    }
  }
  // Use raw pointer coords in layout space and clamp into the surface.
  if (surf_w > 0) {
    sx = std::clamp(sx, 0.0, surf_w - 1.0);
  }
  if (surf_h > 0) {
    sy = std::clamp(sy, 0.0, surf_h - 1.0);
  }
  return { sx, sy };
}

static bool
wayland_pointer_focus_requested(WlrServer* server)
{
  if (!server || !server->engine) {
    return false;
  }
  if (server->seat) {
    // If the seat already has a focused surface (pointer or keyboard), treat that
    // as an active Wayland focus so we keep routing input and avoid flicker.
    if (server->seat->pointer_state.focused_surface) {
      return true;
    }
    if (server->seat->keyboard_state.focused_surface) {
      return true;
    }
  }
  // If ImGui wants the mouse, honor that as a pointer focus request.
  if (server->engine->imguiWantsMouse()) {
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
ensure_pointer_focus(WlrServer* server, uint32_t time_msec = 0, wlr_surface* preferred = nullptr)
{
  if (!server || !server->seat) {
    return;
  }
  // Prefer an existing focused surface if present.
  wlr_surface* surf = preferred ? preferred : server->seat->pointer_state.focused_surface;
  // If WM has a different focused Wayland app, prefer that surface so hotkey
  // focus also updates pointer focus.
  if (server->engine && server->registry) {
    if (auto wm = server->engine->getWindowManager()) {
      if (auto focused = wm->getCurrentlyFocusedApp();
          focused && server->registry->valid(*focused) &&
          server->registry->all_of<WaylandApp::Component>(*focused)) {
        auto& comp = server->registry->get<WaylandApp::Component>(*focused);
        wlr_surface* wmSurf = comp.app ? comp.app->getSurface() : nullptr;
        if (wmSurf && wmSurf != surf) {
          surf = wmSurf;
        }
      }
    }
  }
  if (!surf) {
    if (server->engine && server->registry) {
      if (auto wm = server->engine->getWindowManager()) {
        if (auto focused = wm->getCurrentlyFocusedApp();
            focused && server->registry->valid(*focused) &&
            server->registry->all_of<WaylandApp::Component>(*focused)) {
          auto& comp = server->registry->get<WaylandApp::Component>(*focused);
          if (comp.app) {
            surf = comp.app->getSurface();
          }
        }
      }
    }
    // Fallback: pick the first known Wayland surface.
    if (!surf && !server->surface_map.empty()) {
      surf = server->surface_map.begin()->first;
    }
  }
  if (!surf) {
    return;
  }
  auto mapped = map_pointer_to_surface(server, surf);
  double sx = mapped.first;
  double sy = mapped.second;
  if (time_msec == 0) {
    uint64_t now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now().time_since_epoch())
                        .count();
    time_msec = static_cast<uint32_t>(now_ms & 0xffffffff);
  }
  wlr_seat_pointer_notify_enter(server->seat, surf, sx, sy);
  wlr_seat_pointer_notify_motion(server->seat, time_msec, sx, sy);
  wlr_seat_pointer_notify_frame(server->seat);
}

static std::pair<wlr_surface*, entt::entity>
pick_any_surface(WlrServer* server)
{
  if (!server) {
    return { nullptr, entt::null };
  }
  if (!server->surface_map.empty()) {
    auto it = server->surface_map.begin();
    return { it->first, it->second };
  }
  return { nullptr, entt::null };
}
static void
set_cursor_visible(WlrServer* server, bool visible, wlr_output* output = nullptr)
{
  if (!server || !server->cursor) {
    return;
  }
  if (server->engine) {
    if (auto wm = server->engine->getWindowManager()) {
      // If WM tracks a visibility override, respect it.
      if (auto sp = wm->getCursorVisibleOverride()) {
        visible = *sp;
      }
    }
  }
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
is_menu_surface(const std::shared_ptr<WaylandApp>& app)
{
  if (!app) {
    return false;
  }
  std::string name = app->getWindowName();
  std::string lowered = name;
  std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return lowered.find("rofi") != std::string::npos || lowered.find("wofi") != std::string::npos ||
         lowered.find("menu") != std::string::npos;
}

static bool
isHotkeySym(const WlrServer* server, xkb_keysym_t sym)
{
  if (!server) {
    return false;
  }
  return is_hotkey_sym(server->hotkeyModifier, sym);
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
                  PendingWlAction{ PendingWlAction::Add,
                                   comp->app,
                                   entry.first,
                                   comp->accessory,
                                   comp->layer_shell,
                                   false,
                                   nullptr,
                                   comp->offset_x,
                                   comp->offset_y,
                                   comp->screen_x,
                                   comp->screen_y });
              }
            }
          }
        }
      }
    }
  }
  auto actions = std::move(server->pending_wl_actions);
  server->pending_wl_actions.clear();
  std::vector<PendingWlAction> retry;
  for (auto& action : actions) {
    if (action.type == PendingWlAction::Add) {
      if (!action.menu_surface && server->engine) {
        if (auto wm = server->engine->getWindowManager()) {
          if (wm->consumeMenuSpawnPending()) {
            action.menu_surface = true;
          }
        }
      }
      // Force menu surfaces into a screen-space layer shell so they render as overlays.
      if (!action.layer_shell && action.menu_surface) {
        action.layer_shell = true;
        action.accessory = true; // skip Positionable placement
        action.screen_x =
          std::max(0, static_cast<int>((SCREEN_WIDTH - action.app->getWidth()) / 2));
        action.screen_y =
          std::max(0, static_cast<int>((SCREEN_HEIGHT - action.app->getHeight()) / 2));
      }
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
      entt::entity parentEnt = entt::null;
      if (action.parent_surface) {
        auto pit = server->surface_map.find(action.parent_surface);
        if (pit != server->surface_map.end()) {
          parentEnt = pit->second;
        }
      }
      // If this is a popup/accessory whose parent hasn't registered yet, retry
      // once the parent arrives.
      if (action.accessory && !action.layer_shell && parentEnt == entt::null) {
        retry.push_back(action);
        continue;
      }
      entt::entity entity = entt::null;
      if (auto wm = server->engine->getWindowManager()) {
        bool spawnAtCamera = !action.accessory;
        entity = wm->registerWaylandApp(action.app,
                                        spawnAtCamera,
                                        action.accessory,
                                        parentEnt,
                                        action.offset_x,
                                        action.offset_y,
                                        action.layer_shell,
                                        action.screen_x,
                                        action.screen_y,
                                        action.screen_w,
                                        action.screen_h);
        if (entity != entt::null) {
          if (auto* comp = server->registry->try_get<WaylandApp::Component>(entity)) {
            action.app = comp->app;
            comp->accessory = action.accessory;
            comp->parent = parentEnt;
            comp->offset_x = action.offset_x;
            comp->offset_y = action.offset_y;
            comp->layer_shell = action.layer_shell;
            comp->screen_x = action.screen_x;
            comp->screen_y = action.screen_y;
            comp->screen_w = action.screen_w > 0 ? action.screen_w : comp->screen_w;
            comp->screen_h = action.screen_h > 0 ? action.screen_h : comp->screen_h;
          }
        }
      }
      if (entity == entt::null && server->registry) {
        entity = server->registry->create();
        server->registry->emplace<WaylandApp::Component>(
          entity,
          action.app,
          action.accessory,
          action.layer_shell,
          parentEnt,
          action.offset_x,
          action.offset_y,
          action.screen_x,
          action.screen_y,
          action.screen_w,
          action.screen_h);
      }
      if (entity != entt::null) {
        auto* comp = server->registry->try_get<WaylandApp::Component>(entity);
        const char* name = comp && comp->app ? comp->app->getWindowName().c_str() : "(null)";
        if ((action.accessory || action.layer_shell) && action.parent_surface == nullptr &&
            !action.menu_surface) {
        }
        // Always give menu surfaces immediate focus/input so they can receive keystrokes.
        if (is_menu_surface(action.app) && server->engine) {
          if (auto wm = server->engine->getWindowManager()) {
            wm->unfocusApp();
            wm->focusApp(entity);
            if (comp && comp->app) {
              comp->app->takeInputFocus();
            }
          }
        }
        if (renderer && comp && comp->app) {
          bool needsTexture =
            comp->app->getTextureId() <= 0 || comp->app->getTextureUnit() < 0;
          if (needsTexture) {
            renderer->registerApp(comp->app.get());
          }
        }
        server->surface_map[action.surface] = entity;
        if (server->registry) {
          auto view = server->registry->view<WaylandApp::Component>();
        }
        // Layer-shell menus should take focus immediately so they receive keystrokes.
        if (action.layer_shell && server->engine) {
          if (auto wm = server->engine->getWindowManager()) {
            auto focused = wm->getCurrentlyFocusedApp();
            bool allowFocus = (action.parent_surface != nullptr) || action.menu_surface;
            if (allowFocus) {
              wm->unfocusApp();
              wm->focusApp(entity);
              if (comp && comp->app) {
                comp->app->takeInputFocus();
              }
            }
          }
        }
        // Request a default window size that matches the X11 defaults.
        if (comp && comp->app && !action.accessory) {
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
    } else {
      if (f) {
        std::fprintf(f,
                     "wayland app action unknown (deferred): surface=%p\n",
                     (void*)action.surface);
        std::fflush(f);
      }
    }
  }
  // Requeue any popup actions that lacked a registered parent when first seen.
  if (!retry.empty()) {
    server->pending_wl_actions.insert(
      server->pending_wl_actions.end(), retry.begin(), retry.end());
  }
  if (f) {
    std::fclose(f);
  }
}

struct WaylandAppHandle {
  WlrServer* server = nullptr;
  std::shared_ptr<WaylandApp> app;
  wlr_xdg_surface* xdg_surface = nullptr;
  wlr_surface* surface = nullptr;
  bool accessory = false;
  wlr_surface* parent_surface = nullptr;
  int offset_x = 0;
  int offset_y = 0;
  wl_listener destroy;
  wl_listener unmap;
  wl_listener commit;
  bool registered = false;
  entt::entity entity = entt::null;
  bool unmapLinked = false;
};

struct LayerSurfaceHandle {
  WlrServer* server = nullptr;
  std::shared_ptr<WaylandApp> app;
  wlr_layer_surface_v1* layer = nullptr;
  wl_listener commit;
  wl_listener destroy;
  wl_listener unmap;
  bool registered = false;
  bool configured = false;
  entt::entity entity = entt::null;
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

  if (sym == XKB_KEY_equal || sym == XKB_KEY_plus || sym == XKB_KEY_minus ||
      sym == XKB_KEY_underscore || sym == XKB_KEY_0 || sym == XKB_KEY_9) {
  }

  if (isHotkeySym(server, sym)) {
    if (pressed) {
      ++server->replayModifierHeld;
    } else if (server->replayModifierHeld > 0) {
      --server->replayModifierHeld;
    }
    server->replayModifierActive = server->replayModifierHeld > 0;
  }
  if (sym == XKB_KEY_Shift_L || sym == XKB_KEY_Shift_R) {
    if (pressed) {
      ++server->replayShiftHeld;
      server->pendingReplayShift = true;
    } else if (server->replayShiftHeld > 0) {
      --server->replayShiftHeld;
      server->pendingReplayShift = false;
    }
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
    if (isHotkeySym(server, sym)) {
      if (pressed) {
        depressed |= server->hotkeyModifierMask;
      } else {
        // Only clear modifier when no other matching key is logically held.
        if (server->replayModifierHeld <= 0) {
          depressed &= ~server->hotkeyModifierMask;
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
      std::optional<entt::entity> focusCandidate = wm->getPendingFocusedApp();
      if (!focusCandidate) {
        focusCandidate = wm->getCurrentlyFocusedApp();
      }
      if (focusCandidate) {
        if (server->registry &&
            server->registry->all_of<WaylandApp::Component>(*focusCandidate)) {
          waylandFocusActive = true;
        }
      }
    }
  }

  if (pressed && server->engine) {
    Controls* controls = server->engine->getControls();
    uint32_t mods = keyboard ? wlr_keyboard_get_modifiers(keyboard) : 0;
    bool modifierHeld =
      (mods & server->hotkeyModifierMask) || server->replayModifierActive;
    bool shiftHeld = (mods & WLR_MODIFIER_SHIFT) || server->pendingReplayShift ||
                     server->replayShiftHeld > 0;
    if (sym == XKB_KEY_equal || sym == XKB_KEY_plus || sym == XKB_KEY_minus ||
        sym == XKB_KEY_underscore || sym == XKB_KEY_0 || sym == XKB_KEY_9) {
    }
    if (controls) {
      auto resp =
        controls->handleKeySym(sym, pressed, modifierHeld, shiftHeld, waylandFocusActive);
      bool handled = resp.consumed || resp.blockClientDelivery || resp.requestQuit ||
                     resp.clearInputForces || resp.clearSeatFocus;
      
      if (resp.clearInputForces) {
        clear_input_forces(server);
      }
      if (resp.clearSeatFocus && server->seat) {
        wlr_seat_keyboard_notify_clear_focus(server->seat);
        wlr_seat_pointer_notify_clear_focus(server->seat);
        server->replayModifierActive = false;
      }
      if (resp.requestQuit) {
        wl_display_terminate(server->display);
        return;
      }
      if (resp.blockClientDelivery) {
        blockClientDelivery = true;
      }
      if (resp.consumed) {
        return;
      }
    }
  }
  switch (sym) {
    case XKB_KEY_w:
    case XKB_KEY_W:
      if (!waylandFocusActive) {
        server->input.forward = pressed;
      } else if (!pressed) {
        server->input.forward = false;
      }
      break;
    case XKB_KEY_s:
    case XKB_KEY_S:
      if (!waylandFocusActive) {
        server->input.back = pressed;
      } else if (!pressed) {
        server->input.back = false;
      }
      break;
    case XKB_KEY_a:
    case XKB_KEY_A:
      if (!waylandFocusActive) {
        server->input.left = pressed;
      } else if (!pressed) {
        server->input.left = false;
      }
      break;
    case XKB_KEY_d:
    case XKB_KEY_D:
      if (!waylandFocusActive) {
        server->input.right = pressed;
      } else if (!pressed) {
        server->input.right = false;
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
    bool duplicate = false;
    if (keycode != 0) {
      if (pressed) {
        if (server->input.pressed_keys.count(keycode) > 0) {
          duplicate = true;
        } else {
          server->input.pressed_keys.insert(keycode);
        }
      } else {
        server->input.pressed_keys.erase(keycode);
      }
    }
    if (duplicate) {
      return;
    }
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
      bool focusChanged =
        server->seat->keyboard_state.focused_surface != target_surface;
      // Keep WM focus in sync with the seat when we can map the surface.
      if (server->engine) {
        if (auto wm = server->engine->getWindowManager()) {
          auto it = server->surface_map.find(target_surface);
          if (it != server->surface_map.end()) {
            // Do not force WM focus here; explicit hotkeys handle focus.
            if (focusChanged) {
              if (auto* controls = server->engine->getControls()) {
                controls->clearMovementInput();
              }
              clear_input_forces(server);
            }
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
    }
    enum wl_keyboard_key_state state =
      pressed ? WL_KEYBOARD_KEY_STATE_PRESSED : WL_KEYBOARD_KEY_STATE_RELEASED;
    wlr_seat_keyboard_notify_key(server->seat, time_msec, keycode, state);
    wlr_seat_keyboard_notify_modifiers(server->seat, &keyboard->modifiers);
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
  if (nsyms > 0) {
    bool pressed = event->state == WL_KEYBOARD_KEY_STATE_PRESSED;
    process_key_sym(
      server, handle->keyboard, syms[0], pressed, event->time_msec, event->keycode);
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
          if (handle->server->engine) {
            handle->server->engine->updateImGuiPointer(handle->server->pointer_x,
                                                       handle->server->pointer_y,
                                                       handle->server->input.mouse_buttons);
          }
        }
        bool focusRequested = wayland_pointer_focus_requested(handle->server);
        if (focusRequested) {
          auto surfEnt = pick_any_surface(handle->server);
          auto* surf = surfEnt.first;
          ensure_pointer_focus(handle->server, event->time_msec, surf);
        }
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
          if (handle->server->engine) {
            handle->server->engine->updateImGuiPointer(handle->server->pointer_x,
                                                       handle->server->pointer_y,
                                                       handle->server->input.mouse_buttons);
          }
        }
        bool focusRequested = wayland_pointer_focus_requested(handle->server);
        if (focusRequested) {
          auto surfEnt = pick_any_surface(handle->server);
          auto* surf = surfEnt.first;
          ensure_pointer_focus(handle->server, event->time_msec, surf);
        }
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
      handle->axis.notify = [](wl_listener* listener, void* data) {
        auto* handle =
          wl_container_of(listener, static_cast<WlrPointerHandle*>(nullptr), axis);
        auto* event = static_cast<wlr_pointer_axis_event*>(data);
        wlr_surface* preferred_surface = nullptr;
        if (handle->server && handle->server->seat &&
            handle->server->seat->pointer_state.focused_surface) {
          preferred_surface = handle->server->seat->pointer_state.focused_surface;
        }
        if (!preferred_surface) {
          auto picked = pick_any_surface(handle->server);
          preferred_surface = picked.first;
        }
        ensure_pointer_focus(handle->server, event->time_msec, preferred_surface);
        if (handle->server && handle->server->seat) {
          wlr_seat_pointer_notify_axis(handle->server->seat,
                                       event->time_msec,
                                       event->orientation,
                                       event->delta,
                                       event->delta_discrete,
                                       event->source,
                                       event->relative_direction);
          wlr_seat_pointer_notify_frame(handle->server->seat);
        }
      };
      wl_signal_add(&pointer->events.axis, &handle->axis);
      handle->button.notify = [](wl_listener* listener, void* data) {
        auto* handle =
          wl_container_of(listener, static_cast<WlrPointerHandle*>(nullptr), button);
        auto* event = static_cast<wlr_pointer_button_event*>(data);
        bool handled_by_game = false;
        bool wayland_focus_requested = wayland_pointer_focus_requested(handle->server);
        wlr_surface* pointer_surface = nullptr;
        if (handle->server && handle->server->seat) {
          pointer_surface = handle->server->seat->pointer_state.focused_surface;
        }
        wlr_surface* preferred_surface = nullptr;
        Controls* controls =
          handle->server && handle->server->engine
            ? handle->server->engine->getControls()
            : nullptr;
        auto update_mouse_button = [&](uint32_t button, bool pressed) {
          if (!handle->server) {
            return;
          }
          if (button == BTN_LEFT) {
            handle->server->input.mouse_buttons[0] = pressed;
          } else if (button == BTN_RIGHT) {
            handle->server->input.mouse_buttons[1] = pressed;
          } else if (button == BTN_MIDDLE) {
            handle->server->input.mouse_buttons[2] = pressed;
          }
          if (handle->server->engine) {
            handle->server->engine->updateImGuiPointer(handle->server->pointer_x,
                                                       handle->server->pointer_y,
                                                       handle->server->input.mouse_buttons);
          }
        };
        if (static_cast<uint32_t>(event->state) ==
              static_cast<uint32_t>(WLR_BUTTON_PRESSED) &&
            handle->server && handle->server->engine) {
          update_mouse_button(event->button, true);
          // If a Wayland client has (or is requesting) focus, bypass game controls so
          // the click reaches the client immediately.
          if (controls && !wayland_focus_requested) {
            handled_by_game = controls->handlePointerButton(event->button, true);
          }
          if (auto wm = handle->server->engine->getWindowManager()) {
            // Only focus a Wayland client when the player is looking at it up close;
            // otherwise treat the click as a game interaction.
            if (!handled_by_game) {
              // Focus only when the player is actually looking at an app within range.
              entt::entity focusEnt = entt::null;
              wlr_surface* focusSurf = nullptr;
              if (auto space = wm->getSpace()) {
                if (auto looked = space->getLookedAtApp()) {
                  entt::entity ent = *looked;
                  if (handle->server->registry && handle->server->registry->valid(ent) &&
                      handle->server->registry->all_of<WaylandApp::Component>(ent)) {
                    auto* comp = handle->server->registry->try_get<WaylandApp::Component>(ent);
                    entt::entity targetEnt = ent;
                    if (comp && comp->accessory && comp->parent != entt::null &&
                        handle->server->registry->valid(comp->parent)) {
                      targetEnt = comp->parent;
                      comp = handle->server->registry->try_get<WaylandApp::Component>(targetEnt);
                    }
                    if (comp && comp->app) {
                      focusEnt = targetEnt;
                      focusSurf = comp->app->getSurface();
                    }
                  }
                }
              }
              if (focusEnt != entt::null && focusSurf) {
                wm->focusApp(focusEnt);
                ensure_pointer_focus(handle->server, event->time_msec, focusSurf);
                if (auto* focusComp =
                      handle->server->registry->try_get<WaylandApp::Component>(focusEnt)) {
                  if (focusComp->app) {
                    focusComp->app->takeInputFocus();
                  }
                }
                preferred_surface = focusSurf;
              }
            }
          }
        }
        if (handle->server->seat && !handled_by_game) {
          // Only forward to Wayland clients if a surface currently has pointer focus.
          wlr_surface* surf = preferred_surface ? preferred_surface : pointer_surface;
          if (surf) {
            ensure_pointer_focus(handle->server, event->time_msec, surf);
            auto mapped = map_pointer_to_surface(handle->server, surf);
            wlr_seat_pointer_notify_motion(handle->server->seat,
                                           event->time_msec,
                                           mapped.first,
                                           mapped.second);
            wlr_seat_pointer_notify_button(handle->server->seat,
                                           event->time_msec,
                                           event->button,
                                           event->state);
            wlr_seat_pointer_notify_frame(handle->server->seat);
            if (static_cast<uint32_t>(event->state) ==
                static_cast<uint32_t>(WLR_BUTTON_RELEASED)) {
              update_mouse_button(event->button, false);
            }
          }
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
        wl_list_remove(&handle->axis.link);
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
  bool is_popup =
    xdg_surface && xdg_surface->role == WLR_XDG_SURFACE_ROLE_POPUP;
  wlr_surface* parent_surface = nullptr;
  int offset_x = 0;
  int offset_y = 0;
  if (is_popup && xdg_surface && xdg_surface->popup) {
    parent_surface = xdg_surface->popup->parent;
    if (parent_surface) {
      double sx = 0.0;
      double sy = 0.0;
      wlr_xdg_popup_get_position(xdg_surface->popup, &sx, &sy);
      offset_x = static_cast<int>(sx);
      offset_y = static_cast<int>(sy);
    } else {
      // Treat popups without a parent as non-accessory to avoid crashes.
      is_popup = false;
    }
  }
  // Heuristic: only treat popups/override-redirect style surfaces as accessory
  // if they're small (menus, tooltips). Large surfaces (e.g., OBS main window)
  // should be normal apps.
  if (is_popup && xdg_surface && xdg_surface->surface) {
    int w = xdg_surface->surface->current.width;
    int h = xdg_surface->surface->current.height;
    int maxAccessoryW = static_cast<int>(SCREEN_WIDTH / 2);
    int maxAccessoryH = static_cast<int>(SCREEN_HEIGHT / 2);
    if (w > maxAccessoryW || h > maxAccessoryH) {
      is_popup = false;
    }
  }
  auto app = std::make_shared<WaylandApp>(server->renderer,
                                          server->allocator,
                                          xdg_surface,
                                          0,
                                          /*request_initial_size=*/!is_popup);
  app->setSeat(server->seat, xdg_surface->surface);

  auto* handle = new WaylandAppHandle();
  handle->server = server;
  handle->app = app;
  handle->xdg_surface = xdg_surface;
  handle->surface = xdg_surface->surface;
  handle->accessory = is_popup;
  handle->parent_surface = parent_surface;
  handle->offset_x = offset_x;
  handle->offset_y = offset_y;
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
    if (handle->xdg_surface && handle->xdg_surface->popup) {
      handle->parent_surface = handle->xdg_surface->popup->parent;
      if (handle->parent_surface) {
        double sx = 0.0;
        double sy = 0.0;
        wlr_xdg_popup_get_position(handle->xdg_surface->popup, &sx, &sy);
        handle->offset_x = static_cast<int>(sx);
        handle->offset_y = static_cast<int>(sy);
      } else {
        handle->accessory = false;
        handle->offset_x = 0;
        handle->offset_y = 0;
      }
    }
    // Defer registration to the render loop to avoid GL/context races.
    handle->server->pending_wl_actions.push_back(
      PendingWlAction{ PendingWlAction::Add,
                       handle->app,
                       surf,
                       handle->accessory,
                       false,
                       false,
                       handle->parent_surface,
                       handle->offset_x,
                       handle->offset_y,
                       0,
                       0 });
    handle->registered = true;
  };
  wl_signal_add(&xdg_surface->surface->events.commit, &handle->commit);
  handle->unmap.notify = [](wl_listener* listener, void* /*data*/) {
    auto* handle = wl_container_of(listener, static_cast<WaylandAppHandle*>(nullptr), unmap);
    if (handle->server && handle->surface) {
      handle->server->pending_wl_actions.push_back(
        PendingWlAction{ PendingWlAction::Remove, handle->app, handle->surface });
      handle->registered = false;
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

static std::pair<int, int>
compute_layer_shell_position(WlrServer* server,
                             wlr_layer_surface_v1* layer,
                             int width,
                             int height)
{
  int out_w = SCREEN_WIDTH;
  int out_h = SCREEN_HEIGHT;
  if (server && server->primary_output) {
    out_w = server->primary_output->width;
    out_h = server->primary_output->height;
  }
  auto state = layer ? layer->current : wlr_layer_surface_v1_state{};
  bool anchor_left = state.anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT;
  bool anchor_right = state.anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
  bool anchor_top = state.anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP;
  bool anchor_bottom = state.anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM;

  int x = 0;
  int y = 0;
  if (anchor_left && !anchor_right) {
    x = state.margin.left;
  } else if (anchor_right && !anchor_left) {
    x = out_w - width - state.margin.right;
  } else {
    x = std::max(0, (out_w - width) / 2);
  }

  if (anchor_top && !anchor_bottom) {
    y = state.margin.top;
  } else if (anchor_bottom && !anchor_top) {
    y = out_h - height - state.margin.bottom;
  } else {
    y = std::max(0, (out_h - height) / 2);
  }
  return { x, y };
}

static void
handle_new_layer_surface(wl_listener* listener, void* data)
{
  auto* server =
    wl_container_of(listener, static_cast<WlrServer*>(nullptr), new_layer_surface);
  auto* layer = static_cast<wlr_layer_surface_v1*>(data);
  if (!server || !layer) {
    return;
  }
  if (!layer->output && server->primary_output) {
    layer->output = server->primary_output;
  }

  auto* handle = new LayerSurfaceHandle();
  handle->server = server;
  handle->layer = layer;

  handle->commit.notify = [](wl_listener* listener, void* data) {
    auto* handle =
      wl_container_of(listener, static_cast<LayerSurfaceHandle*>(nullptr), commit);
    auto* surf = static_cast<wlr_surface*>(data);
    if (!handle || !handle->server || !surf) {
      return;
    }
    if (!handle->configured) {
      uint32_t out_w = handle->server->primary_output->width;
      uint32_t out_h = handle->server->primary_output->height;
      uint32_t desired_w =
        handle->layer && handle->layer->current.desired_width > 0
          ? handle->layer->current.desired_width
          : out_w;
      uint32_t desired_h =
        handle->layer && handle->layer->current.desired_height > 0
          ? handle->layer->current.desired_height
          : out_h;
      wlr_layer_surface_v1_configure(handle->layer, desired_w, desired_h);
      handle->configured = true;
      return;
    }
    if (handle->registered) {
      return;
    }
    int w = surf->current.width;
    int h = surf->current.height;
    if (w <= 0 || h <= 0) {
      return;
    }
    auto pos = compute_layer_shell_position(handle->server, handle->layer, w, h);
    auto app = std::make_shared<WaylandApp>(handle->server->renderer,
                                            handle->server->allocator,
                                            surf,
                                            "layer-shell",
                                            0);
    if (handle->server->seat) {
      app->setSeat(handle->server->seat, surf);
    }
    handle->app = app;
    handle->server->pending_wl_actions.push_back(
      PendingWlAction{ PendingWlAction::Add,
                       app,
                       surf,
                       true,
                       true,
                       false,
                       nullptr,
                       0,
                       0,
                       pos.first,
                       pos.second,
                       handle->server->primary_output ? handle->server->primary_output->width
                                                      : static_cast<int>(SCREEN_WIDTH),
                       handle->server->primary_output ? handle->server->primary_output->height
                                                      : static_cast<int>(SCREEN_HEIGHT) });
    handle->registered = true;
  };
  wl_signal_add(&layer->surface->events.commit, &handle->commit);

  handle->unmap.notify = [](wl_listener* listener, void* /*data*/) {
    auto* handle =
      wl_container_of(listener, static_cast<LayerSurfaceHandle*>(nullptr), unmap);
    if (handle && handle->server && handle->app && handle->layer) {
      handle->server->pending_wl_actions.push_back(
        PendingWlAction{ PendingWlAction::Remove, handle->app, handle->layer->surface });
      handle->registered = false;
    }
    if (handle) {
      handle->configured = false;
    }
  };
  wl_signal_add(&layer->surface->events.unmap, &handle->unmap);

  handle->destroy.notify = [](wl_listener* listener, void* data) {
    auto* handle =
      wl_container_of(listener, static_cast<LayerSurfaceHandle*>(nullptr), destroy);
    auto* surf = static_cast<wlr_surface*>(data);
    if (handle && handle->server) {
      handle->server->pending_wl_actions.push_back(
        PendingWlAction{ PendingWlAction::Remove,
                         handle->app,
                         surf,
                         true,
                         true,
                         false,
                         nullptr,
                         0,
                         0,
                         0,
                         0 });
      handle->registered = false;
      ensure_wayland_apps_registered(handle->server);
    }
    safe_remove_listener(&handle->unmap);
    safe_remove_listener(&handle->destroy);
    safe_remove_listener(&handle->commit);
    delete handle;
  };
  wl_signal_add(&layer->surface->events.destroy, &handle->destroy);
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
  } else if (xdg_surface->role == WLR_XDG_SURFACE_ROLE_POPUP) {
    // Popups need an initial configure to define their anchor/geometry.
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
    if (handle->xdg->role != WLR_XDG_SURFACE_ROLE_TOPLEVEL &&
        handle->xdg->role != WLR_XDG_SURFACE_ROLE_POPUP) {
      return;
    }
    if (!handle->configured_sent) {
      if (handle->xdg->role == WLR_XDG_SURFACE_ROLE_TOPLEVEL && handle->xdg->toplevel) {
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
    if (!handle->created) {
      handle->created = true;
      create_wayland_app(handle->server, handle->xdg);
    }
  };
  wl_signal_add(&xdg_surface->surface->events.map, &handle->map);

  handle->destroy.notify = [](wl_listener* listener, void* data) {
    auto* handle = wl_container_of(listener, static_cast<XdgSurfaceHandle*>(nullptr), destroy);
    auto* xdg = static_cast<wlr_xdg_surface*>(data);
    handle->xdg = nullptr;
    wl_list_remove(&handle->map.link);
    wl_list_remove(&handle->commit.link);
    wl_list_remove(&handle->destroy.link);
    delete handle;
  };
  wl_signal_add(&xdg_surface->events.destroy, &handle->destroy);
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
  } else {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
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

  // no-op if the swapchain has already been configured
  // only errors if old swapchain isn't configured properly
  // and a new one couldn't be created & configured
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
  ensure_glad(server);

  GLuint fbo = wlr_gles2_renderer_get_buffer_fbo(server->renderer, buffer);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  ensure_depth_buffer(handle, width, height, fbo);
  if (!server->engine) {
    EngineOptions options;
    options.enableControls = true; // wlroots path feeds input through Controls
    options.enableGui = true; // draw HackMatrix ImGui overlay inside compositor
    options.invertYAxis = true;
    server->engine = std::make_unique<Engine>(nullptr, server->envp, options);
    // Surface API quit handling through wl_display so tests can terminate
    // cleanly.
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
    Controls* controls = server->engine->getControls();
    Camera* camera = server->engine->getCamera();
    if (camera) {
      if (controls) {
        controls->applyMovementInput(
          server->input.forward, server->input.back, server->input.left, server->input.right);
      } else {
        camera->handleTranslateForce(
          server->input.forward, server->input.back, server->input.left, server->input.right);
      }
      bool pointerFocusRequested = wayland_pointer_focus_requested(server);
      bool cursorOverrideVisible = false;
      bool cursorOverrideSet = false;
      if (server->engine) {
        if (auto wm = server->engine->getWindowManager()) {
          if (auto ov = wm->getCursorVisibleOverride()) {
            cursorOverrideSet = true;
            cursorOverrideVisible = *ov;
          }
        }
      }
      bool pointerVisible = pointerFocusRequested || (cursorOverrideSet && cursorOverrideVisible);
      if (!pointerVisible &&
          (server->input.delta_x != 0.0 || server->input.delta_y != 0.0)) {
        if (controls) {
          controls->applyLookDelta(server->input.delta_x, -server->input.delta_y);
        } else {
          camera->handleRotateForce(
            nullptr, server->input.delta_x, -server->input.delta_y);
        }
      }
      // Hide cursor and clear pointer focus when game mode is active so only deltas matter.
      if (!pointerVisible && server->seat &&
          server->seat->pointer_state.focused_surface) {
        wlr_seat_pointer_notify_clear_focus(server->seat);
      }
      set_cursor_visible(server, pointerVisible, handle->output);
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
  double frameStart = currentTimeSeconds();
  server->engine->frame(frameStart);
  // Run screenshot capture after the frame has been drawn so the image matches
  // what was just rendered.
  if (server->engine && server->engine->getWindowManager() &&
      server->engine->getRenderer()) {
    auto wm = server->engine->getWindowManager();
    if (wm && wm->consumeScreenshotRequest()) {
      if (auto* renderer = server->engine->getRenderer()) {
        renderer->screenshotFromCurrentFramebuffer(width, height, fbo);
      }
    }
  }

  // Render software cursors (clients set them via wl_pointer.set_cursor).
  pixman_region32_t cursor_damage;
  pixman_region32_init_rect(&cursor_damage, 0, 0, width, height);
  wlr_output_add_software_cursors_to_render_pass(handle->output, pass, &cursor_damage);
  pixman_region32_fini(&cursor_damage);
  // Draw a compositor-owned software cursor when either a Wayland surface has focus
  // or the WM explicitly requested visibility (e.g., toggle_cursor hotkey).
  bool cursorVisibleOverride = false;
  if (server->engine) {
    if (auto wm = server->engine->getWindowManager()) {
      if (auto ov = wm->getCursorVisibleOverride()) {
        cursorVisibleOverride = *ov;
      }
    }
  }
  if (server->engine && (wayland_pointer_focus_requested(server) || cursorVisibleOverride)) {
    if (auto* renderer = server->engine->getRenderer()) {
      float sizePx = 24.0f * (handle->output ? handle->output->scale : 1.0f);
      renderer->renderSoftwareCursor(server->pointer_x, server->pointer_y, sizePx);
    }
  }

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

// This is called when wayland detects a new output such as a monitor
// It sets up size, cursor info, and frame notify handler (which will render each frame)
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
  } else if (wlr_output_mode* mode = wlr_output_preferred_mode(output)) {
    wlr_output_state_set_mode(&state, mode);
  }
  if (!wlr_output_commit_state(output, &state)) {
    wlr_log(WLR_ERROR, "Failed to commit output %s", output->name);
    wlr_output_state_finish(&state);
    return;
  }
  wlr_output_state_finish(&state);

  // Keep global screen dimensions in sync with the wlroots output so renderer
  // projection and texture sizing match the real buffer size.
  SCREEN_WIDTH = static_cast<float>(output->width);
  SCREEN_HEIGHT = static_cast<float>(output->height);

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


const char*
hotkey_modifier_label(HotkeyModifier mod)
{
  return mod == HotkeyModifier::Alt ? "alt" : "super";
}

HotkeyModifier
parse_hotkey_modifier()
{
  std::string mod = "alt";
  try {
    mod = Config::singleton()->get<std::string>("key_mappings.super_modifier");
  } catch (...) {
    // Default to alt when the key is missing.
  }
  std::transform(mod.begin(), mod.end(), mod.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  if (mod == "super" || mod == "logo" || mod == "mod4" || mod == "win") {
    return HotkeyModifier::Super;
  }
  return HotkeyModifier::Alt;
}

uint32_t
hotkey_modifier_mask(HotkeyModifier mod)
{
  return mod == HotkeyModifier::Alt ? WLR_MODIFIER_ALT : WLR_MODIFIER_LOGO;
}

void initialize_wlr_logging() { wlr_log_init(WLR_DEBUG, nullptr); }

// --- Main bootstrap helpers -------------------------------------------------

void
apply_backend_env_defaults()
{
  // If WLR_BACKENDS not set, prefer wayland when under Wayland, otherwise try
  // X11 so running from an X session or tty can still work.
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
    if (!std::getenv("WLR_X11_OUTPUT_WIDTH") || !std::getenv("WLR_X11_OUTPUT_HEIGHT")) {
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
    }
  }
}

bool
WlrServer::create_display()
{
  display = wl_display_create();
  if (!display) {
    std::fprintf(stderr, "Failed to create Wayland display\n");
    return false;
  }
  return true;
}

bool
WlrServer::create_backend() {
  backend =
    wlr_backend_autocreate(wl_display_get_event_loop(display), nullptr);
  if (!backend) {
    std::fprintf(stderr, "Failed to create wlroots backend\n");
    return false;
  }
  return true;
}

bool
WlrServer::create_renderer()
{
  renderer = wlr_renderer_autocreate(backend);
  if (!renderer) {
    std::fprintf(stderr, "Failed to create renderer\n");
    return false;
  }
  if (!wlr_renderer_init_wl_display(renderer, display)) {
    std::fprintf(stderr, "Failed to init renderer for wl_display\n");
    return false;
  }
  return true;
}

bool
WlrServer::create_allocator() {

  allocator = wlr_allocator_autocreate(backend, renderer);
  if (!allocator) {
    std::fprintf(stderr, "Failed to create allocator\n");
    return false;
  }

  // Detect X11 backend (common for nested testing).
  const char* backend_env = std::getenv("WLR_BACKENDS");
  if (backend_env && std::string(backend_env).find("x11") != std::string::npos) {
    isX11Backend = true;
  } else if (!std::getenv("WAYLAND_DISPLAY") && std::getenv("DISPLAY")) {
    isX11Backend = true;
  }
  const char* backend_kind = "unknown";
  if (isX11Backend) {
    backend_kind = "x11";
  } else if (std::getenv("WAYLAND_DISPLAY")) {
    backend_kind = "wayland";
  }
  return true;
}

bool
WlrServer::create_seat() {
  seat = wlr_seat_create(display, "seat0");
  if (seat) {
    wlr_seat_set_capabilities(seat,
                              WL_SEAT_CAPABILITY_POINTER | WL_SEAT_CAPABILITY_KEYBOARD);
    // Prefer the default theme, but allow fallback later if it is missing.
    cursor_mgr = wlr_xcursor_manager_create("default", 24);
    if (cursor_mgr) {
      wlr_xcursor_manager_load(cursor_mgr, 1);
    }
    cursor = wlr_cursor_create();
    if (cursor && output_layout) {
      wlr_cursor_attach_output_layout(cursor, output_layout);
    }
    request_set_cursor.notify = [](wl_listener* listener, void* data) {
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
      } else {
        set_cursor_visible(server, wayland_pointer_focus_requested(server));
      }
      server->pointer_x = server->cursor->x;
      server->pointer_y = server->cursor->y;
    };
    wl_signal_add(&seat->events.request_set_cursor, &request_set_cursor);
    // Set an initial default cursor.
    set_cursor_visible(this, wayland_pointer_focus_requested(this));
    return true;
  } else {
    std::fprintf(stderr, "Failed to create seat");
    return false;
  }
}

bool
WlrServer::init_protocols()
{
  output_layout = wlr_output_layout_create(display);

  compositor = wlr_compositor_create(display, 5, renderer);
  if (!compositor) {
    std::fprintf(stderr, "Failed to create compositor\n");
    return false;
  }
  // Some clients (e.g., foot) require wl_subcompositor to be advertised.
  if (!wlr_subcompositor_create(display)) {
    std::fprintf(stderr, "Failed to create subcompositor\n");
    return false;
  }
  layer_shell = wlr_layer_shell_v1_create(display, 4);
  if (!layer_shell) {
    std::fprintf(stderr, "Failed to create layer-shell\n");
    return false;
  }
  xdg_shell = wlr_xdg_shell_create(display, 3);
  if (!xdg_shell) {
    std::fprintf(stderr, "Failed to create xdg-shell\n");
    return false;
  }
  
  data_device_manager = wlr_data_device_manager_create(display);
  // Advertise the screencopy protocol so tools like grim can capture frames.
  screencopy_manager = wlr_screencopy_manager_v1_create(display);
  if (output_layout) {
    xdg_output_manager =
      wlr_xdg_output_manager_v1_create(display, output_layout);
  }
  return true;
}

void
WlrServer::register_listeners()
{
  new_layer_surface.notify = handle_new_layer_surface;
  wl_signal_add(&layer_shell->events.new_surface, &new_layer_surface);

  new_xdg_surface.notify = handle_new_xdg_surface;
  wl_signal_add(&xdg_shell->events.new_surface, &new_xdg_surface);

  new_output.notify = handle_new_output;
  wl_signal_add(&backend->events.new_output, &new_output);

  new_input.notify = handle_new_input;
  wl_signal_add(&backend->events.new_input, &new_input);
}

bool
WlrServer::start_backend_and_socket()
{
  if (!wlr_backend_start(backend)) {
    std::fprintf(stderr, "Failed to start wlroots backend\n");
    return false;
  }
  const char* socket = wl_display_add_socket_auto(display);
  if (!socket) {
    std::fprintf(stderr, "Failed to create Wayland socket\n");
    return false;
  }
  setenv("WAYLAND_DISPLAY", socket, true);

  wlr_log(WLR_DEBUG,
          "wlroots compositor ready; WAYLAND_DISPLAY=%s",
          socket ? socket : "(null)");

  wlr_log(WLR_DEBUG,
          "wlroots compositor ready; WAYLAND_DISPLAY=%s",
          socket ? socket : "(null)");
  return true;
}

bool WlrServer::init_resources()
{
  if (!create_display() || !create_backend() || !create_renderer() ||
      !create_allocator() || !init_protocols() || !create_seat()) {
    return false;
  }
  register_listeners();
  return true;
}

bool 
WlrServer::start() {
  if (!start_backend_and_socket()) {
    return false;
  }
  wl_display_run(display);
  return true;
}

WlrServer::WlrServer(char** envp): envp(envp) {
  hotkeyModifier = parse_hotkey_modifier();
  hotkeyModifierMask = hotkey_modifier_mask(hotkeyModifier);

  initialize_wlr_logging();
  apply_backend_env_defaults();
}

WlrServer::~WlrServer()
{
  auto remove_listener = [](wl_listener& listener) {
    if (listener.link.prev || listener.link.next) {
      wl_list_remove(&listener.link);
    }
  };
  if (engine) {
    engine.reset();
  }
  if (seat) {
    remove_listener(request_set_cursor);
  }
  if (virtual_keyboard) {
    wlr_keyboard_finish(virtual_keyboard);
    delete virtual_keyboard;
    virtual_keyboard = nullptr;
  }
  remove_listener(new_input);
  remove_listener(new_output);
  remove_listener(new_layer_surface);
  remove_listener(new_xdg_surface);
  if (allocator) {
    wlr_allocator_destroy(allocator);
  }
  if (renderer) {
    wlr_renderer_destroy(renderer);
  }
  if (backend) {
    wlr_backend_destroy(backend);
  }
  if (display) {
    wl_display_destroy(display);
  }
}
