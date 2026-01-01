#include <future>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-enum-enum-conversion"
#define ENET_IMPLEMENTATION
#include <enet/enet.h>
#pragma GCC diagnostic pop

#include "engine.h"
#include "components/Bootable.h"
#include "components/Key.h"
#include "components/Lock.h"
#include "components/Parent.h"
#include "components/Scriptable.h"
#include "components/Light.h"
#include "entity.h"
#include "logger.h"
#include "model.h"
#include "persister.h"
#include "systems/Boot.h"
#include "systems/Derivative.h"
#include "systems/Light.h"
#include "WindowManager/WindowManager.h"
#include "blocks.h"
#include "systems/Door.h"
#include "tracy/Tracy.hpp"
#include "tracy/TracyOpenGL.hpp"

#include <memory>
#include <spdlog/common.h>
#include <cstdlib>
#include <chrono>
#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <fstream>

#include "imgui/imgui_impl_opengl3.h"

namespace {

double
currentTimeSeconds()
{
  static const auto start = std::chrono::steady_clock::now();
  const auto now = std::chrono::steady_clock::now();
  std::chrono::duration<double> elapsed = now - start;
  return elapsed.count();
}

} // namespace

void
mouseCallback(GLFWwindow* window, double xpos, double ypos)
{
  Engine* engine = (Engine*)glfwGetWindowUserPointer(window);
  engine->controls->mouseCallback(window, xpos, ypos);
}

// The engineGui can't be created until the callback is registered
// Encapsulate in Engine, even though it uses glfw
void
Engine::registerCursorCallback()
{
  glfwSetWindowUserPointer(window, (void*)this);
  glfwSetCursorPosCallback(window, mouseCallback);
}

void
Engine::setupRegistry()
{
  registry = make_shared<EntityRegistry>();

  // Create all the Persistor
  // declaritively, because C++ type system can't 
  // iterate through the type parameters
  
  shared_ptr<SQLPersister> postionablePersister =
    make_shared<PositionablePersister>(registry);
  registry->addPersister(postionablePersister);

  shared_ptr<SQLPersister> modelPersister =
    make_shared<ModelPersister>(registry);
  registry->addPersister(modelPersister);

  shared_ptr<SQLPersister> lightPersister =
    make_shared<LightPersister>(registry);
  registry->addPersister(lightPersister);

  shared_ptr<SQLPersister> doorPersister =
    make_shared<systems::DoorPersister>(registry);
  registry->addPersister(doorPersister);

  shared_ptr<SQLPersister> keyPersister = make_shared<KeyPersister>(registry);
  registry->addPersister(keyPersister);

  shared_ptr<SQLPersister> lockPersister = make_shared<LockPersister>(registry);
  registry->addPersister(lockPersister);

  shared_ptr<SQLPersister> parentPersister =
    make_shared<ParentPersister>(registry);
  registry->addPersister(parentPersister);

  shared_ptr<SQLPersister> scriptablePersister =
    make_shared<ScriptablePersister>(registry);
  registry->addPersister(scriptablePersister);

  shared_ptr<SQLPersister> bootablePersister =
    make_shared<BootablePersister>(registry);
  registry->addPersister(bootablePersister);

  registry->createTablesIfNeeded();
  registry->loadAll();
}

shared_ptr<LoggerVector> Engine::setupLogger() {
  auto loggerVector = make_shared<LoggerVector>();
  auto imGuiSink = make_shared<ImGuiSink>(loggerVector);
  loggerSink = make_shared<LoggerSink>(fileSink, imGuiSink);
  logger = make_shared<spdlog::logger>("engine", loggerSink);
  logger->set_level(spdlog::level::debug);
  return loggerVector;
}

Engine::Engine(GLFWwindow* window, char** envp, EngineOptions options)
  : window(window)
  , envp(envp)
  , options(options)
  , frameTimes(20, 0.0)
{
  setupRegistry();

  // this probably doesn't belong here
  // candidate for refactor
  systems::createDerivativeComponents(registry);

  auto loggerVector = setupLogger();
  initializeMemberObjs();

  if (window != nullptr) {
    glfwFocusWindow(window);
  }

  TracyGpuContext;

  wire();
  if (wm) {
    wm->createAndRegisterApps(envp);
  }

  if (options.enableControls && window != nullptr) {
    registerCursorCallback();
  }
  // Has to be be created after the cursorCallback because gui wraps the
  // callback
  if (options.enableGui && window != nullptr) {
    engineGui = make_shared<EngineGui>(this, window, registry, loggerVector);
  }
}

Engine::~Engine()
{
  // may want to remove this because it might be slow on shutdown
  // when trying to get fast dev time
  if (controls) {
    delete controls;
  }
  delete renderer;
  delete world;
  delete camera;
  //delete api;
  registry->saveAll();
}

void
Engine::action(Action action)
{
  if (world) {
    world->action(action);
  }
}

void
Engine::initializeMemberObjs()
{
  const char* apiAddressEnv = std::getenv("VOXEL_API_BIND");
  std::string apiAddress =
    apiAddressEnv != nullptr ? apiAddressEnv : "tcp://*:4455";
  if (const char* logPath = std::getenv("MATRIX_WLROOTS_OUTPUT")) {
    std::ofstream out(logPath, std::ios::app);
    out << "engine: VOXEL_API_BIND=" << apiAddress << "\n";
  }

  auto texturePack = blocks::initializeBasicPack();
  // Default to X11 WM only when we have a GLFW X11 window; wlroots path uses a
  // Wayland-aware WM. For wlroots, matrix window is null so we skip X11 setup.
  if (window != nullptr) {
    wm = make_shared<WindowManager::WindowManager>(
      registry, glfwGetX11Window(window), loggerSink, envp);
  } else {
    // Wayland-only path (wlroots) doesn't have a GLFW window; build a WM in
    // headless mode so we can still place/render apps.
    wm = make_shared<WindowManager::WindowManager>(registry, loggerSink, true, envp);
  }
  camera = new Camera();
  world = new World(
    registry, camera, texturePack, true, loggerSink);
  renderer = new Renderer(registry, camera, world, texturePack, options.invertYAxis);
  camera->setInvertY(options.invertYAxis);
  if (options.enableControls) {
    controls = new Controls(wm, world, camera, renderer, texturePack);
  } else {
    controls = nullptr;
  }
  api = new Api(apiAddress, registry, controls, renderer, world, wm);
  if (wm) {
    wm->registerControls(controls);
  }
}

void
Engine::wire()
{
  world->attachRenderer(renderer);
  if (wm) {
    wm->wire(wm, camera, renderer);
  }
}

void Engine::multiplayerClientIteration(double frameStart) {

  static double lastPlayerUpdate = 0;
  if (client) {
    client->poll();
  }

  if (client && frameStart - lastPlayerUpdate > 1.0 / 20.0) {
    client->sendPlayer(camera->position, camera->front);
    lastPlayerUpdate = frameStart;
  }
}

void
Engine::loop()
{
  // Skip shadow-map lighting pass while experimenting with voxel outlines to
  // avoid crashes in the lighting pipeline.
  // systems::updateLighting(registry, renderer);
  try {
    while (!glfwWindowShouldClose(window)) {
      TracyGpuZone("loop");
      glfwPollEvents();
      double frameStart = currentTimeSeconds();
      frame(frameStart);
      glfwSwapBuffers(window);
    }
  } catch (const std::exception& e) {
    logger->error(e.what());
    throw;
  }
}

void
Engine::frame(double frameStart)
{
  if (api != nullptr) {
    api->mutateEntities();
  }

  renderer->render();

  if (engineGui) {
    engineGui->render(fps, frameIndex, frameTimes);
  }

  // this has the potential to make OpenGL calls (for lighting; 1 render
  // call per light)
  world->tick();

  //api->mutateEntities();
  if (wm) {
    wm->tick();
  }

  if (engineGui) {
    disableKeysIfImguiActive();
  }
  if (controls && window != nullptr) {
    controls->poll(window, camera, world);
  }
  multiplayerClientIteration(frameStart);

  if (engineGui) {
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
  }

  TracyGpuCollect;
  FrameMark;

  frameTimes[frameIndex] = currentTimeSeconds() - frameStart;
  frameIndex = (frameIndex + 1) % frameTimes.size();
  if (frameIndex == 0 && frameTimes.size() > 0) {
    fps = 0.0;
    for (double ft : frameTimes) {
      fps += ft;
    }
    fps /= static_cast<double>(frameTimes.size());
    if (fps > 0.0) {
      fps = 1.0 / fps;
    }
  }
}

void
Engine::disableKeysIfImguiActive() {
  if(ImGui::IsAnyItemActive()) {
    controls->disableKeys();
  } else {
    controls->enableKeys();
  }
}

void
Engine::registerClient(shared_ptr<MultiPlayer::Client> _client)
{
  client = _client;
}

void
Engine::registerServer(shared_ptr<MultiPlayer::Server> _server)
{
  server = _server;
}

shared_ptr<EntityRegistry>
Engine::getRegistry()
{
  return registry;
}
