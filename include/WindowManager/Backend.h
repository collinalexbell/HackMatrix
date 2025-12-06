#pragma once

#include <optional>
#include <memory>
#include <entt/entity/entity.hpp>

class Controls;
class Renderer;
class Camera;
class World;

namespace WindowManager {

// Thin interface to allow multiple windowing backends (X11, Wayland, etc).
class Backend {
public:
  virtual ~Backend() = default;

  virtual void registerControls(Controls* controls) = 0;
  virtual void createAndRegisterApps(char** envp) = 0;
  virtual void focusApp(entt::entity entity) = 0;
  virtual void unfocusApp() = 0;
  virtual std::optional<entt::entity> getCurrentlyFocusedApp() = 0;
  virtual void tick() = 0;
};

} // namespace WindowManager
