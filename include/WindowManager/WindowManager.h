#pragma once
#include "WindowManager/Space.h"
#include "app.h"
#include "entity.h"
#include "logger.h"
#include "world.h"
#include "components/Bootable.h"
#include <X11/Xlib.h>
#include <X11/extensions/Xcomposite.h>
#include <atomic>
#include <map>
#include <memory>
#include <spdlog/common.h>
#include <thread>

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
class WindowManager {
  shared_ptr<EntityRegistry> registry;
  Display *display = NULL;
  Controls *controls = NULL;
  spdlog::sink_ptr logSink;
  int screen;
  entt::entity emacs;
  entt::entity magicaVoxel;
  entt::entity microsoftEdge;
  entt::entity obs;
  entt::entity terminator;

  IdeSelection ideSelection;

  vector<entt::entity> appsWithHotKeys;
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
  void forkOrFindApp(string cmd, string pidOf, string className, entt::entity&,
                     char **envp, string args = "");
  std::thread substructureThread;
  bool continueRunning = true;
  void removeAppForWindow(Window);
  void onMapRequest(XMapRequestEvent);
  std::shared_ptr<spdlog::logger> logger;
  void onHotkeyPress(XKeyEvent event);
  void createApp(Window window, unsigned int width = Bootable::DEFAULT_WIDTH,
                 unsigned int height = Bootable::DEFAULT_HEIGHT);
  void addApp(X11App *, entt::entity);
  void allow_input_passthrough(Window window);
  void capture_input(Window window, bool shapeBounding, bool shapeInput);
  void addApps();
  void createUnfocusHackThread(entt::entity entity);
  int waitForRemovalChangeSize (int curSize);
  void logWaitForRemovalChangeSize(int changeSize);
  void adjustAppsToAddAfterAdditions(vector<X11App*>& waitForRemoval);
  void setWMProps(Window root);
  void reconfigureWindow(XConfigureEvent);

public:
  void unfocusApp();
  void dMenu();
  void passthroughInput();
  void captureInput();
  void createAndRegisterApps(char **envp);
  WindowManager(shared_ptr<EntityRegistry>, Window, spdlog::sink_ptr);
  ~WindowManager();
  optional<entt::entity> getCurrentlyFocusedApp();
  void focusApp(entt::entity);
  void wire(shared_ptr<WindowManager>, Camera *camera, Renderer *renderer);
  void handleSubstructure();
  void goToLookedAtApp();
  void registerControls(Controls *controls);
  void tick();
};

} // namespace WindowManager
