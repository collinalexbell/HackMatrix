#pragma once

#include <glm/glm.hpp>
#include <string>
#include <atomic>
#include <array>
#include <optional>
#include <wayland-server-core.h>
#include <memory>
#include "AppSurface.h"

struct wlr_surface;
struct wlr_xdg_surface;
struct wlr_xdg_toplevel;
struct wlr_renderer;
struct wlr_allocator;
struct wl_listener;
struct wlr_buffer;
struct wlr_seat;

// Minimal Wayland-backed app placeholder. Mirrors the public surface API of
// X11App so renderer/WindowManager can be driven by a backend abstraction.
class WaylandApp : public AppSurface {
  wlr_surface* surface = nullptr;
  wlr_xdg_surface* xdg_surface = nullptr;
  wlr_xdg_toplevel* xdg_toplevel = nullptr;
  wlr_renderer* renderer = nullptr;
  wlr_allocator* allocator = nullptr;
  wl_listener surface_commit;
  wl_listener surface_destroy;
  wlr_seat* seat = nullptr;
  wlr_surface* seat_surface = nullptr;

  std::optional<wlr_buffer*> pending_buffer;
  bool mapped = false;
  std::string title = "wayland-app";

  int textureUnit = -1;
  int textureId = -1;
  std::atomic_bool focused = false;
  std::atomic_bool selected = false;
  size_t appIndex = 0;
  int x = 0;
  int y = 0;
  int uploadedWidth = 0;
  int uploadedHeight = 0;
  bool sampleLogged = false;

public:
  int width = 0;
  int height = 0;
  glm::mat4 heightScalar = glm::mat4(1.0f);
  struct Component {
    std::shared_ptr<WaylandApp> app;
  };

  WaylandApp(wlr_renderer* renderer,
             wlr_allocator* allocator,
             wlr_xdg_surface* xdg,
             size_t index);
  ~WaylandApp();

  void attachTexture(int unit, int id, size_t index) override
  {
    textureUnit = unit;
    textureId = id;
    appIndex = index;
  }

  // Upload/copy the latest Wayland buffer into the attached texture.
  void appTexture() override;

  void select() override { selected = true; }
  void deselect() override { selected = false; }
  bool isSelected() override { return selected; }

  void focus(unsigned long /*matrix*/) override { focused = true; }
  void unfocus(unsigned long /*matrix*/) override { focused = false; }
  void takeInputFocus() override;
  void setSeat(wlr_seat* seat, wlr_surface* surface)
  {
    this->seat = seat;
    this->seat_surface = surface;
  }
  bool isFocused() override { return focused; }

  void resize(int w, int h) override
  {
    width = w;
    height = h;
  }
  void resizeMove(int w, int h, int newX, int newY) override
  {
    resize(w, h);
    x = newX;
    y = newY;
  }

  int getWidth() const override { return width; }
  int getHeight() const override { return height; }
  std::array<int, 2> getPosition() const override { return { x, y }; }
  glm::mat4 getHeightScalar() const override { return heightScalar; }
  int getTextureId() const override { return textureId; }
  int getTextureUnit() const override { return textureUnit; }
  wlr_surface* getSurface() const { return surface; }

  std::string getWindowName() override { return title; }
  int getPID() override { return 0; }

  size_t getAppIndex() const override { return appIndex; }
  void close();

private:
  void handle_commit(wlr_surface* surface);
  void handle_destroy();
  void update_height_scalar();
};
