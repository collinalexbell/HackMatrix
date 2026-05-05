#pragma once

#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <unordered_set>
#include <vector>
#include <glad/glad.h>
#include <xkbcommon/xkbcommon.h>
#include <memory>
#include "blocks.h"
#include "camera.h"
#include "app.h"
#include "world.h"
#include "WindowManager/WindowManager.h"
#include "ControlMappings.h"

class TypedKeyOverlay;

struct DeferedAction
{
  shared_ptr<bool> isDone;
  function<void()> fn;
};

struct ControlResponse
{
  bool handledByWM = false;
  bool shouldExit = false;
};

class Controls
{
  ControlMappings controlMappings;
  shared_ptr<blocks::TexturePack> texturePack;
  WindowManager::WindowManagerPtr wm;
  shared_ptr<EntityRegistry> registry;
  float windowFlop = 0.25;
  float windowFlop_dt = 0.002;
  World* world;
  Camera* camera;
  Renderer* renderer;
  bool grabbedCursor = true;
  bool appFocused = false;
  double lastClickTime = 0;
  double lastKeyPressTime = 0;
  double lastShiftPressTime = 0;
  bool resetMouse = true;
  bool lastWaylandFocusActive = false;
  bool waylandModifierHeld = false;
  bool waylandShiftHeld = false;
  float lastX;
  float lastY;
  int clickY = 100;
  vector<DeferedAction> deferedActions;
  std::vector<std::function<void()>> queuedActions;
  std::mutex queuedActionsMutex;
  shared_ptr<WindowManager::Space> windowManagerSpace;
  bool keysEnabled = true;

  std::unordered_set<xkb_keysym_t> pressed;

  struct KeysymEvent {
    xkb_keysym_t sym;
    bool pressed;
    bool modifierHeld;
    bool shiftHeld;
  };
  std::queue<KeysymEvent> keysymQueue;
  std::mutex keysymMutex;
  std::shared_ptr<TypedKeyOverlay> typedKeyOverlay;

  void handleMovement();
  void handleModEscape();
  void handleToggleCursor();
  void handleToggleApp();
  void handleSpawnTerminal();
  void handleReloadConfig();
  void handleFocus();
  bool handleUnfocusApp();
  bool handleCloseFocusedApp();
  void handleDMenu();
  void handleScreenshot();
  void handleSave();
  void handleSelection();
  void handleCodeBlock();
  void handleDebug();
  void handleToggleMeshing();
  void handleToggleWireframe();
  void handleLogBlockCounts();
  void handleLogBlockType();
  void handleChangePlayerSpeed();
  void handleWindowFlop();
  bool handleWaylandHotkeys();
  bool isPressed(xkb_keysym_t sym) const;
  bool isPressedEither(xkb_keysym_t a, xkb_keysym_t b) const;

  void handleKeys();
  void doAfter(shared_ptr<bool> isDone, function<void()> actionFn);
  void doDeferedActions();
  void enqueueAction(std::function<void()> fn);

public:
  Controls(WindowManager::WindowManagerPtr wm,
           World* world,
           Camera* camera,
           Renderer* renderer,
           std::shared_ptr<TypedKeyOverlay> typedKeyOverlay,
           shared_ptr<EntityRegistry> registry,
           shared_ptr<blocks::TexturePack> texturePack)
    : wm(wm)
    , world(world)
    , camera(camera)
    , renderer(renderer)
    , typedKeyOverlay(std::move(typedKeyOverlay))
    , texturePack(texturePack)
    , registry(registry)
  {
    lastWaylandFocusActive = false;
  }
  void poll();
  void pollPressedKeys();
  void goToApp(entt::entity);
  void moveTo(glm::vec3 pos,
              optional<glm::vec3> rotation,
              float secs,
              optional<function<void()>> = nullopt);
  bool matchesConfiguredKey(xkb_keysym_t sym, const std::string fn);
  void runQueuedActions();
  ControlResponse handleKeySym(xkb_keysym_t sym,
                               bool pressed,
                               bool modifierHeld,
                               bool shiftHeld);
  void resetWaylandInputState();
  void applyMovementInput(bool forward, bool back, bool left, bool right);
  void clearMovementInput();
  void applyLookDelta(double dx, double dy);
  bool handlePointerButton(uint32_t button, bool pressed);
  void disableKeys();
  void enableKeys();
  void triggerScreenshot();
  void wireWindowManager(shared_ptr<WindowManager::Space>);
};
