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

#include <memory>
#include <spdlog/common.h>
#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include "imgui/imgui_impl_opengl3.h"


void mouseCallback(GLFWwindow *window, double xpos, double ypos) {
  Engine *engine = (Engine *)glfwGetWindowUserPointer(window);
  engine->controls->mouseCallback(window, xpos, ypos);
}

void Engine::registerCursorCallback() {
  glfwSetWindowUserPointer(window, (void *)this);
  glfwSetCursorPosCallback(window, mouseCallback);
}

void Engine::setupRegistry() {
  registry = make_shared<EntityRegistry>();

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

  shared_ptr<SQLPersister> keyPersister =
    make_shared<KeyPersister>(registry);
  registry->addPersister(keyPersister);

  shared_ptr<SQLPersister> lockPersister =
    make_shared<LockPersister>(registry);
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
}

Engine::Engine(GLFWwindow* window, char** envp): window(window) {
  setupRegistry();
  registry->loadAll();
  systems::createDerivativeComponents(registry);
  auto loggerVector = make_shared<LoggerVector>();
  auto imGuiSink = make_shared<ImGuiSink>(loggerVector);
  loggerSink = make_shared<LoggerSink>(fileSink, imGuiSink);
  logger = make_shared<spdlog::logger>("engine", loggerSink);
  logger->set_level(spdlog::level::debug);
  initialize();
  glfwFocusWindow(window);
  wire();
  wm->createAndRegisterApps(envp);
  registerCursorCallback();
  // Has to be be created after the cursorCallback because gui wraps the callback
  engineGui = make_shared<EngineGui>(this, window, registry, loggerVector);
}

Engine::~Engine() {
  // may want to remove this because it might be slow on shutdown
  // when trying to get fast dev time
  delete wm;
  delete controls;
  delete renderer;
  delete world;
  delete camera;
  delete api;
  registry->saveAll();
}

void Engine::initialize(){
  auto texturePack = blocks::initializeBasicPack();
  wm = new WindowManager::WindowManager(registry, glfwGetX11Window(window), loggerSink);
  camera = new Camera();
  world = new World(registry, camera, texturePack, "/home/collin/midtown/", true, loggerSink);
  renderer = new Renderer(registry, camera, world, texturePack);
  controls = new Controls(wm, world, camera, renderer, texturePack);
  api = new Api("tcp://*:3333", registry, controls);
  wm->registerControls(controls);
}


void Engine::wire() {
  world->attachRenderer(renderer);
  wm->wire(camera, renderer);
}

void Engine::loop() {
  vector<double> frameTimes(20, 0);
  double frameStart;
  int frameIndex = 0;
  double fps;
  double lastPlayerUpdate = 0;
  systems::updateLighting(registry, renderer);
  try {
    while (!glfwWindowShouldClose(window)) {
      frameStart = glfwGetTime();
      glfwPollEvents();

      engineGui->render(fps, frameIndex, frameTimes);
      world->tick();
      renderer->render();
      api->mutateEntities();
      wm->tick();
      controls->poll(window, camera, world);

      if(client) {
        client->poll();
      }

      if(client && frameStart - lastPlayerUpdate > 1.0/20.0) {
        client->sendPlayer(camera->position, camera->front);
        lastPlayerUpdate = frameStart;
      }

      ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

      glfwSwapBuffers(window);
      FrameMark;

      frameTimes[frameIndex] = glfwGetTime() - frameStart;
      frameIndex = (frameIndex + 1) % 10;
    }
  } catch (const std::exception &e) {
    logger->error(e.what());
    throw;
  }
}

void Engine::registerClient(shared_ptr<MultiPlayer::Client> _client) {
  client = _client;
}

void Engine::registerServer(shared_ptr<MultiPlayer::Server> _server) {
  server = _server;
}

shared_ptr<EntityRegistry> Engine::getRegistry() {
  return registry;
}
