#pragma once
#include "WindowManager/Space.h"
#include "app.h"
#include "wayland_app.h"
#include "entity.h"
#include "logger.h"
#include "world.h"
#include "components/Bootable.h"
#include <xkbcommon/xkbcommon.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xcomposite.h>
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
class WindowManagerInterface
{
public:
  virtual ~WindowManagerInterface() = default;
  virtual void unfocusApp() = 0;
  virtual void menu() = 0;
  virtual void passthroughInput() = 0;
  virtual void captureInput() = 0;
  virtual void createAndRegisterApps(char** envp) = 0;
  virtual optional<entt::entity> getCurrentlyFocusedApp() = 0;
  virtual void focusApp(entt::entity) = 0;
  virtual void wire(shared_ptr<WindowManagerInterface>, Camera* camera, Renderer* renderer) = 0;
  virtual void handleSubstructure() = 0;
  virtual void goToLookedAtApp() = 0;
  virtual void focusLookedAtApp() = 0;
  virtual std::shared_ptr<Space> getSpace() = 0;
  virtual void registerControls(Controls* controls) = 0;
  virtual void tick() = 0;
  virtual void handleHotkeySym(xkb_keysym_t sym, bool superHeld, bool shiftHeld) = 0;
  virtual void keyReplay(const std::vector<std::pair<std::string, uint32_t>>& entries) = 0;
  virtual std::vector<ReplayEvent> consumeReadyReplaySyms(uint64_t now_ms) = 0;
  virtual bool hasPendingReplay() const = 0;
  virtual bool consumeScreenshotRequest() = 0;
  virtual void requestScreenshot() = 0;
  virtual entt::entity registerWaylandApp(std::shared_ptr<WaylandApp> app,
                                          bool spawnAtCamera = true,
                                          bool accessory = false,
                                          entt::entity parent = entt::null,
                                          int offsetX = 0,
                                          int offsetY = 0) = 0;
};

using WindowManagerPtr = std::shared_ptr<WindowManagerInterface>;

class WindowManager : public WindowManagerInterface
{
  shared_ptr<EntityRegistry> registry;
  Display* display = NULL;
  bool waylandMode = false;
  Controls* controls = NULL;
  spdlog::sink_ptr logSink;
  int screen;
  entt::entity emacs;
  entt::entity magicaVoxel;
  entt::entity microsoftEdge;
  entt::entity obs;
  entt::entity terminator;
  std::string menuProgram;
  char** envp = nullptr;

  IdeSelection ideSelection;

  vector<optional<entt::entity>> appsWithHotKeys;
  optional<entt::entity> currentlyFocusedApp;
  shared_ptr<Space> space;
  Window matrix;
  Window overlay;
  atomic_bool firstRenderComplete = false;
  map<Window, entt::entity> dynamicApps;
  mutex renderLoopMutex;
  mutex continueMutex;
  vector<X11App*> appsToAdd;
  vector<entt::entity> appsToRemove;
  vector<WindowEvent> events;
  void forkOrFindApp(string cmd,
                     string pidOf,
                     string className,
                     entt::entity&,
                     char** envp,
                     string args = "");
  std::thread substructureThread;
  bool continueRunning = true;
  void removeAppForWindow(Window);
  void onMapRequest(XMapRequestEvent);
  std::shared_ptr<spdlog::logger> logger;
  void setupLogger();
  void onHotkeyPress(XKeyEvent event);
  void createApp(Window window,
                 unsigned int width = Bootable::DEFAULT_WIDTH,
                 unsigned int height = Bootable::DEFAULT_HEIGHT);
  void addApp(X11App*, entt::entity);
  void allow_input_passthrough(Window window);
  void capture_input(Window window, bool shapeBounding, bool shapeInput);
  void createUnfocusHackThread(entt::entity entity);
  int waitForRemovalChangeSize(int curSize);
  void logWaitForRemovalChangeSize(int changeSize);
  void adjustAppsToAddAfterAdditions(vector<X11App*>& waitForRemoval);
  void setWMProps(Window root);
  void pruneInvalidFocus();
  void reconfigureWindow(XConfigureEvent);
  void swapHotKeys(int a, int b);
  bool computeAppCameraTarget(entt::entity ent,
                              glm::vec3& targetPos,
                              glm::vec3& rotationDegrees,
                              const char* reasonTag);
  std::shared_ptr<bool> moveCameraToApp(entt::entity ent, const char* reasonTag = "moveCameraToApp");
  void focusEntityAfterMove(entt::entity ent);
  int findAppsHotKey(entt::entity theApp);
  std::vector<ReplayEvent> replayQueue;
  size_t replayIndex = 0;
  bool replayActive = false;
  std::chrono::steady_clock::time_point replayStart;
  std::atomic_bool screenshotRequested = false;

public:
  void unfocusApp() override;
  void menu() override;
  void passthroughInput() override;
  void captureInput() override;
  void createAndRegisterApps(char** envp) override;
  WindowManager(shared_ptr<EntityRegistry>, Window, spdlog::sink_ptr, char** envp = nullptr);
  WindowManager(shared_ptr<EntityRegistry>, spdlog::sink_ptr, bool waylandMode, char** envp = nullptr);
  ~WindowManager();
  optional<entt::entity> getCurrentlyFocusedApp() override;
  void focusApp(entt::entity) override;
  void wire(WindowManagerPtr sharedThis, Camera* camera, Renderer* renderer) override;
  void handleSubstructure() override;
  void goToLookedAtApp() override;
  void focusLookedAtApp() override;
  std::shared_ptr<Space> getSpace() override { return space; }
  void registerControls(Controls* controls) override;
  void tick() override;
  void handleHotkeySym(xkb_keysym_t sym, bool superHeld, bool shiftHeld) override;
  void keyReplay(const std::vector<std::pair<std::string, uint32_t>>& entries) override;
  std::vector<ReplayEvent> consumeReadyReplaySyms(uint64_t now_ms) override;
  bool hasPendingReplay() const override { return replayActive; }
  bool consumeScreenshotRequest() override;
  void requestScreenshot() override;
  // Wayland-only: register a surface-backed app through the WM for placement
  // and rendering.
  entt::entity registerWaylandApp(std::shared_ptr<WaylandApp> app,
                                  bool spawnAtCamera = true,
                                  bool accessory = false,
                                  entt::entity parent = entt::null,
                                  int offsetX = 0,
                                  int offsetY = 0) override;

private:
  Renderer* renderer = nullptr;
  Camera* camera = nullptr;
};

} // namespace WindowManager
