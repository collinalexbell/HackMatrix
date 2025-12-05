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
#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include "imgui/imgui_impl_opengl3.h"

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

Engine::Engine(GLFWwindow* window, char** envp)
  : window(window)
{
  setupRegistry();

  // this probably doesn't belong here
  // candidate for refactor
  systems::createDerivativeComponents(registry);

  auto loggerVector = setupLogger();
  initializeMemberObjs();

  glfwFocusWindow(window);

  TracyGpuContext;

  wire();
  //wm->createAndRegisterApps(envp);

  registerCursorCallback();
  // Has to be be created after the cursorCallback because gui wraps the
  // callback
  engineGui = make_shared<EngineGui>(this, window, registry, loggerVector);
}

Engine::~Engine()
{
  // may want to remove this because it might be slow on shutdown
  // when trying to get fast dev time
  delete controls;
  delete renderer;
  delete world;
  delete camera;
  //delete api;
  registry->saveAll();
}

void
Engine::initializeMemberObjs()
{
  const char* apiAddressEnv = std::getenv("VOXEL_API_BIND");
  std::string apiAddress =
    apiAddressEnv != nullptr ? apiAddressEnv : "tcp://*:4455";

  auto texturePack = blocks::initializeBasicPack();
  //wm = make_shared<WindowManager::WindowManager>(
  //registry, glfwGetX11Window(window), loggerSink);
  camera = new Camera();
  world = new World(
    registry, camera, texturePack, true, loggerSink);
  renderer = new Renderer(registry, camera, world, texturePack);
  controls = new Controls(world, camera, renderer, texturePack);
  api = new Api(apiAddress, registry, controls, renderer, world, wm);
  //wm->registerControls(controls);
}

void
Engine::wire()
{
  world->attachRenderer(renderer);
  //wm->wire(wm, camera, renderer);
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
  vector<double> frameTimes(20, 0);
  double frameStart;
  int frameIndex = 0;
  double fps;
  // Skip shadow-map lighting pass while experimenting with voxel outlines to
  // avoid crashes in the lighting pipeline.
  // systems::updateLighting(registry, renderer);
  try {
    while (!glfwWindowShouldClose(window)) {
      TracyGpuZone("loop");
      glfwPollEvents();
      frameStart = glfwGetTime();

      if (api != nullptr) {
        api->mutateEntities();
      }
      renderer->render();
      engineGui->render(fps, frameIndex, frameTimes);

      // this has the potential to make OpenGL calls (for lighting; 1 render
      // call per light)
      world->tick();

      //api->mutateEntities();
      //wm->tick();

      disableKeysIfImguiActive();
      controls->poll(window, camera, world);
      multiplayerClientIteration(frameStart);

      ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
      glfwSwapBuffers(window);
      TracyGpuCollect;
      FrameMark;

      frameTimes[frameIndex] = glfwGetTime() - frameStart;
      frameIndex = (frameIndex + 1) % 10;
    }
  } catch (const std::exception& e) {
    logger->error(e.what());
    throw;
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
