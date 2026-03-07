#include "wayland/pointer.h"
#include "wayland_app.h"
#include "engine.h"
#include "wayland/wlr_compositor.h"
extern "C" {
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/util/log.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_compositor.h>
#include <linux/input-event-codes.h>
}

bool _isValidWaylandAppComponent(WlrServer* server, entt::entity entity) {
	return server->registry->valid(entity) &&
		server->registry->all_of<WaylandApp::Component>(entity);
}

// Map the global pointer into surface-local coords assuming the surface is
// centered in the output (matches render overlay) and clamp to surface size.
//
// TODO: Remove the clamp which is causing inputs that aren't on the window to be passed into the window.
static std::pair<double, double>
_map_pointer_to_surface(WlrServer* server, wlr_surface* surf)
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
    if (it != server->surface_map.end() && _isValidWaylandAppComponent(server, it->second)) {
      auto& comp = server->registry->get<WaylandApp::Component>(it->second);
      if (comp.accessory && _isValidWaylandAppComponent(server, comp.parent)) {
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

static void
_ensure_pointer_focus(WlrServer* server, uint32_t time_msec = 0, wlr_surface* preferred = nullptr)
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
  auto mapped = _map_pointer_to_surface(server, surf);
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

bool
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

static std::pair<wlr_surface*, entt::entity>
_pick_any_surface(WlrServer* server)
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

void
handle_pointer_motion(wl_listener* listener, void* data)
{
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
  bool focusRequested = wayland_pointer_focus_requested(handle->server);
  if (focusRequested) {
    auto surfEnt = _pick_any_surface(handle->server);
    auto* surf = surfEnt.first;
    _ensure_pointer_focus(handle->server, event->time_msec, surf);
  }
  wlr_log(WLR_DEBUG,
          "pointer motion rel dx=%.3f dy=%.3f",
          event->delta_x,
          event->delta_y);
}

void
handle_pointer_motion_abs(wl_listener* listener, void* data)
{
  auto* handle = wl_container_of(
    listener, static_cast<WlrPointerHandle*>(nullptr), motion_abs);
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
      handle->server->engine->updateImGuiPointer(
        handle->server->pointer_x,
        handle->server->pointer_y,
        handle->server->input.mouse_buttons);
    }
  }
  bool focusRequested = wayland_pointer_focus_requested(handle->server);
  if (focusRequested) {
    auto surfEnt = _pick_any_surface(handle->server);
    auto* surf = surfEnt.first;
    _ensure_pointer_focus(handle->server, event->time_msec, surf);
  }
  wlr_log(WLR_DEBUG,
          "pointer motion abs dx=%.3f dy=%.3f",
          handle->server->input.delta_x,
          handle->server->input.delta_y);
}

void
handle_pointer_axis(wl_listener* listener, void* data)
{
  auto* handle =
    wl_container_of(listener, static_cast<WlrPointerHandle*>(nullptr), axis);
  auto* event = static_cast<wlr_pointer_axis_event*>(data);
  wlr_surface* preferred_surface = nullptr;
  if (handle->server && handle->server->seat &&
      handle->server->seat->pointer_state.focused_surface) {
    preferred_surface = handle->server->seat->pointer_state.focused_surface;
  }
  if (!preferred_surface) {
    auto picked = _pick_any_surface(handle->server);
    preferred_surface = picked.first;
  }
  _ensure_pointer_focus(handle->server, event->time_msec, preferred_surface);
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
}

void handle_pointer_button(wl_listener* listener, void* data)
{
  auto* handle =
    wl_container_of(listener, static_cast<WlrPointerHandle*>(nullptr), button);
  auto* event = static_cast<wlr_pointer_button_event*>(data);
  bool handled_by_game = false;
  bool wayland_focus_requested =
    wayland_pointer_focus_requested(handle->server);
  wlr_surface* pointer_surface = nullptr;
  if (handle->server && handle->server->seat) {
    pointer_surface = handle->server->seat->pointer_state.focused_surface;
  }
  wlr_surface* preferred_surface = nullptr;
  Controls* controls = handle->server && handle->server->engine
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
      handle->server->engine->updateImGuiPointer(
        handle->server->pointer_x,
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
        // Focus only when the player is actually looking at an app within
        // range.
        entt::entity focusEnt = entt::null;
        wlr_surface* focusSurf = nullptr;
        if (auto space = wm->getSpace()) {
          if (auto looked = space->getLookedAtApp()) {
            entt::entity ent = *looked;
            if (handle->server->registry &&
                handle->server->registry->valid(ent) &&
                handle->server->registry->all_of<WaylandApp::Component>(ent)) {
              auto* comp =
                handle->server->registry->try_get<WaylandApp::Component>(ent);
              entt::entity targetEnt = ent;
              if (comp && comp->accessory && comp->parent != entt::null &&
                  handle->server->registry->valid(comp->parent)) {
                targetEnt = comp->parent;
                comp = handle->server->registry->try_get<WaylandApp::Component>(
                  targetEnt);
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
          _ensure_pointer_focus(handle->server, event->time_msec, focusSurf);
          if (auto* focusComp =
                handle->server->registry->try_get<WaylandApp::Component>(
                  focusEnt)) {
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
      _ensure_pointer_focus(handle->server, event->time_msec, surf);
      auto mapped = _map_pointer_to_surface(handle->server, surf);
      wlr_seat_pointer_notify_motion(
        handle->server->seat, event->time_msec, mapped.first, mapped.second);
      wlr_seat_pointer_notify_button(
        handle->server->seat, event->time_msec, event->button, event->state);
      wlr_seat_pointer_notify_frame(handle->server->seat);
      if (static_cast<uint32_t>(event->state) ==
          static_cast<uint32_t>(WLR_BUTTON_RELEASED)) {
        update_mouse_button(event->button, false);
      }
    }
  }
}

void
handle_pointer_destroy(wl_listener* listener, void* data)
{
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