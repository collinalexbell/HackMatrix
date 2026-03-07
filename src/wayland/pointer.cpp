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
}

bool _isValidWaylandAppComponent(WlrServer* server, entt::entity entity) {
	return server->registry->valid(entity) &&
		server->registry->all_of<WaylandApp::Component>(entity);
}

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

static bool
_wayland_pointer_focus_requested(WlrServer* server)
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

void handle_pointer_motion (wl_listener* listener, void* data) {
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
        bool focusRequested = _wayland_pointer_focus_requested(handle->server);
        if (focusRequested) {
          auto surfEnt = _pick_any_surface(handle->server);
          auto* surf = surfEnt.first;
          _ensure_pointer_focus(handle->server, event->time_msec, surf);
        }
        wlr_log(WLR_DEBUG, "pointer motion rel dx=%.3f dy=%.3f",
                event->delta_x,
                event->delta_y);
      }