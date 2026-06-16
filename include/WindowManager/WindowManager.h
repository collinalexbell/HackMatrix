#pragma once
#include "WindowManager/Space.h"
#include "wayland_app.h"
#include "entity.h"
#include "logger.h"
#include "world.h"
#include "components/Bootable.h"
#include <xkbcommon/xkbcommon.h>
#include <array>
#include <atomic>
#include <string>
#include <map>
#include <memory>
#include <spdlog/common.h>
#include <thread>
#include <optional>
#include <chrono>
#include <vector>

class Controls;

struct IdeSelection {
  entt::entity emacs;
  int emacsPid;

  entt::entity terminator;
  int terminatorPid;

  entt::entity vsCode;
  int vsCodePid;
};

namespace WindowManager {
constexpr size_t kHotkeySlotCount = 10;

enum WINDOW_EVENT_TYPE
{
  SMALLER,
  LARGER
};

struct ReplayEvent {
  xkb_keysym_t sym;
  uint64_t ready_ms;
};
struct WindowEvent
{
  WINDOW_EVENT_TYPE type;
  entt::entity window;
};
class WindowManager;
using WindowManagerPtr = std::shared_ptr<WindowManager>;

class WindowManager
{
  shared_ptr<EntityRegistry> registry;
  Controls* controls = NULL;
  spdlog::sink_ptr logSink;
  entt::entity emacs;
  entt::entity magicaVoxel;
  entt::entity microsoftEdge;
  entt::entity obs;
  entt::entity terminator;
  std::string menuProgram;
  std::string terminalProgram;
  char** envp = nullptr;

  IdeSelection ideSelection;

  optional<entt::entity> currentlyFocusedApp;
  shared_ptr<Space> space;
  atomic_bool firstRenderComplete = false;
  mutex renderLoopMutex;
  mutex continueMutex;
  vector<entt::entity> appsToRemove;
  vector<WindowEvent> events;
  std::thread substructureThread;
  bool continueRunning = true;
  std::shared_ptr<spdlog::logger> logger;
  void setupLogger();
  void assignHotkeySlot(entt::entity ent);
  bool computeFocusedSpawn(entt::entity newApp, glm::vec3& pos, glm::vec3& rot) const;
  void positionRelativeToFocus(entt::entity appEntity);
  bool computeAppCameraTarget(entt::entity ent,
                              glm::vec3& targetPos,
                              glm::vec3& rotationDegrees,
                              const char* reasonTag);
  std::shared_ptr<bool> moveCameraToApp(entt::entity ent, const char* reasonTag = "moveCameraToApp");
  void focusEntityAfterMove(entt::entity ent);
  std::vector<ReplayEvent> replayQueue;
  size_t replayIndex = 0;
  bool replayActive = false;
  std::chrono::steady_clock::time_point replayStart;
  std::atomic_bool screenshotRequested = false;
  std::optional<bool> cursorVisible;
  std::optional<entt::entity> pendingFocusedApp;
  std::optional<entt::entity> cursorInputFocusedApp;
  std::array<std::optional<entt::entity>, kHotkeySlotCount> appsWithHotKeys{};

public:
  vector<optional<entt::entity>> getAppsWithHotKeys() {
    return vector<optional<entt::entity>>(appsWithHotKeys.begin(),
                                          appsWithHotKeys.end());
  }
  optional<entt::entity> getHotkeyTarget(int index);
  void swapHotKeys(int a, int b);
  int findAppsHotKey(entt::entity theApp);
  void releaseHotkeySlot(entt::entity theApp);
  void unfocusApp();
  void menu();
  void launchTerminal();
  void createAndRegisterApps(char** envp);
  WindowManager(shared_ptr<EntityRegistry>, spdlog::sink_ptr, char** envp = nullptr);
  bool isWaylandMode() const { return true; }
  ~WindowManager();
  optional<entt::entity> getCurrentlyFocusedApp();
  optional<entt::entity> getPendingFocusedApp();
  bool hasCurrentOrPendingFocus();
  void focusApp(entt::entity);
  void wire(WindowManagerPtr sharedThis, Camera* camera, Renderer* renderer);
  void setCursorVisible(bool visible);
  std::shared_ptr<Space> getSpace() { return space; }
  void registerControls(Controls* controls);
  bool consumeScreenshotRequest();
  void requestScreenshot();
  // Wayland-only: register a surface-backed app through the WM for placement
  // and rendering.
  entt::entity registerWaylandApp(std::shared_ptr<WaylandApp> app,
                                  bool spawnAtCamera = true,
                                  bool accessory = false,
                                  entt::entity parent = entt::null,
                                  int offsetX = 0,
                                  int offsetY = 0,
                                  bool layerShell = false,
                                  int screenX = 0,
                                  int screenY = 0,
                                  int screenW = 0,
                                  int screenH = 0);
  std::optional<bool> getCursorVisibleOverride() const { return cursorVisible; }
  void setCursorInputFocus(entt::entity appEntity);
  void clearCursorInputFocus();
  optional<entt::entity> getCursorInputFocusedApp();

private:
  Renderer* renderer = nullptr;
  Camera* camera = nullptr;
};

} // namespace WindowManager
