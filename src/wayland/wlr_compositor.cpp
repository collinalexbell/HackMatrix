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
#include <EGL/egl.h>
#include <wayland-server-core.h>
#include <xkbcommon/xkbcommon.h>
#include <linux/input-event-codes.h>

extern "C" {
#include <wlr/backend.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/render/allocator.h>
#include <wlr/render/drm_format_set.h>
#include <wlr/render/gles2.h>
#include <wlr/render/swapchain.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/interfaces/wlr_keyboard.h>
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
#define namespace namespace_
#include <wlr/types/wlr_layer_shell_v1.h>
#undef namespace
#include <wlr/types/wlr_xdg_output_v1.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_output.h>
#include <wlr/util/log.h>
};

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
#include "wayland/pointer.h"
#include "wayland/keyboard.h"

#ifdef WLROOTS_DEBUG_LOGS
constexpr bool kWlrootsDebugLogs = true;
#else
constexpr bool kWlrootsDebugLogs = false;
#endif

// Initialize the externed variables found in screen.h
// Use null values until the server is created.
float SCREEN_WIDTH = 0;
float SCREEN_HEIGHT = 0;

namespace {

static void add_action(WlrServer* server, PendingWlAction action) {
  auto* renderer = server->engine->getRenderer();
if (!action.menu_surface && server->engine) {
        if (auto wm = server->engine->getWindowManager()) {
          if (wm->consumeMenuSpawnPending()) {
            action.menu_surface = true;
          }
        }
      }
      // Drop duplicate adds for the same surface if it's already registered.
      if (server->surface_map.find(action.surface) != server->surface_map.end()) {
        return;
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

      // REMOVING FOR TEST, THIS MAY BREAK SOME THINGS SO KEEPING HERE SO I REMEMBER
      /*
      if (action.accessory && !action.layer_shell && parentEnt == entt::null) {
        retry.push_back(action);
        return;
      }
      */
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
        if (server->engine) {
          if (auto api = server->engine->getApi()) {
            api->forceUpdateCachedStatus();
          }
        }
      }
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

  auto actions = std::move(server->pending_wl_actions);
  server->pending_wl_actions.clear();
  std::vector<PendingWlAction> retry;
  for (auto& action : actions) {
    if (action.type == PendingWlAction::Add) {
      add_action(server, action);
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
    } 
  }
  // Requeue any popup actions that lacked a registered parent when first seen.
  if (!retry.empty()) {
    server->pending_wl_actions.insert(
      server->pending_wl_actions.end(), retry.begin(), retry.end());
  }
}

// Removes a listener from the linked list of listeners
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

void
handle_new_input(wl_listener* listener, void* data)
{
  auto* server =
    wl_container_of(listener, static_cast<WlrServer*>(nullptr), new_input);
  auto* device = static_cast<wlr_input_device*>(data);
  switch(device->type) {
    case WLR_INPUT_DEVICE_POINTER:
      handle_new_pointer(server, device);
      break;
    case WLR_INPUT_DEVICE_KEYBOARD:
      handle_new_keyboard(server, device);
      break;
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
  int x = width;
  int y = height;

  return {0,0};
}

/*
// Primarily positions the Wofi menu (and other overlay style desktop notifications/apps)
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
*/


// This handles desktop level UI, specifically the wofi menu which does not render as a normal app, but as an overlay
// wlr-layer-shell is a Wayland extension that defines how desktop components such as panels, notifications, and backgrounds 
// can be displayed as “layers” above or below application windows.
// These should bypass window management
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


// xdg-shell is a Wayland extension that defines how desktop-style applications create and manage their windows
// these are "normal" applications that are to be rendered using window management
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
    server->engine = std::make_unique<Engine>(server->envp, options);
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
  server->engine->frame();
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
  if (wlr_output_mode* mode = wlr_output_preferred_mode(output)) {
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

// event handler for copy/paste
void handle_set_selection(wl_listener* listener, void* data) {
  auto* server =
        wl_container_of(listener, static_cast<WlrServer*>(nullptr), request_set_selection);
  wlr_seat_request_set_selection_event *event = static_cast<wlr_seat_request_set_selection_event*>(data);
  wlr_seat_set_selection(server->seat, event->source, event->serial);
}

bool
WlrServer::create_seat() {
  seat = wlr_seat_create(display, "seat0");
  if (seat) {
    wlr_seat_set_capabilities(seat, WL_SEAT_CAPABILITY_POINTER | WL_SEAT_CAPABILITY_KEYBOARD);

    // copy/paste
    request_set_selection.notify = handle_set_selection;
    wl_signal_add(&seat->events.request_set_selection, &request_set_selection);

    create_cursor(this);
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
