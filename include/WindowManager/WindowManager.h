#pragma once
#include "WindowManager/Space.h"
#include "app.h"
#include "entity.h"
#include "logger.h"
#include "world.h"
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
  entt::entity terminator;
  entt::entity vsCode;
};

namespace WindowManager {
class WindowManager {
  static constexpr int APP_WIDTH = 1920 * .85;
  static constexpr int APP_HEIGHT = 1920 * .85 * .54;

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

  optional<entt::entity> currentlyFocusedApp;
  shared_ptr<Space> space;
  Window matrix;
  Window overlay;
  atomic_bool firstRenderComplete = false;
  map<Window, entt::entity> dynamicApps;
  mutex renderLoopMutex;
  vector<X11App*> appsToAdd;
  vector<entt::entity> appsToRemove;
  void forkOrFindApp(string cmd, string pidOf, string className, entt::entity&,
                     char **envp, string args = "");
  std::thread substructureThread;
  void onDestroyNotify(XDestroyWindowEvent);
  void onMapRequest(XMapRequestEvent);
  std::shared_ptr<spdlog::logger> logger;
  void onHotkeyPress(XKeyEvent event);
  void unfocusApp();
  void createApp(Window window, unsigned int width = APP_WIDTH,
                    unsigned int height = APP_HEIGHT);
  void allow_input_passthrough(Window window);
  void capture_input(Window window, bool shapeBounding, bool shapeInput);
  void addApps();

public:
  void passthroughInput();
  void captureInput();
  void createAndRegisterApps(char **envp);
  WindowManager(shared_ptr<EntityRegistry>, Window, spdlog::sink_ptr);
  ~WindowManager();
  void focusApp(entt::entity);
  void wire(Camera *camera, Renderer *renderer);
  void handleSubstructure();
  void goToLookedAtApp();
  void registerControls(Controls *controls);
  void tick();
};

} // namespace WindowManager
