#include "entity.h"
#include "logger.h"
#include "model.h"
#include "persister.h"
#include <memory>
#include <spdlog/common.h>
#define GLFW_EXPOSE_NATIVE_X11
#include "engine.h"
#include "blocks.h"
#include "assets.h"
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include "systems/Door.h"

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

  registry->createTablesIfNeeded();
}

Engine::Engine(GLFWwindow* window, char** envp): window(window) {
  setupRegistry();
  registry->loadAll();
  auto loggerVector = make_shared<LoggerVector>();
  auto imGuiSink = make_shared<ImGuiSink>(loggerVector);
  loggerSink = make_shared<LoggerSink>(fileSink, imGuiSink);
  logger = make_shared<spdlog::logger>("engine", loggerSink);
  logger->set_level(spdlog::level::debug);
  initialize();
  wm->createAndRegisterApps(envp);
  glfwFocusWindow(window);
  wire();
  registerCursorCallback();
  // Has to be be created after the cursorCallback because gui wraps the callback
  engineGui = make_shared<EngineGui>(window, registry, loggerVector);
}

Engine::~Engine() {
  // may want to remove this because it might be slow on shutdown
  // when trying to get fast dev time
  registry->saveAll();
  delete controls;
  delete renderer;
  delete world;
  delete camera;
  //delete wm;
  delete api;
}

void Engine::initialize(){
  auto texturePack = blocks::initializeBasicPack();
  wm = new WM(glfwGetX11Window(window), loggerSink);
  camera = new Camera();
  world = new World(registry, camera, texturePack, "/home/collin/midtown/", true, loggerSink);
  api = new Api("tcp://*:3333", world);
  renderer = new Renderer(registry, camera, world, texturePack);
  controls = new Controls(wm, world, camera, renderer, texturePack);
  wm->registerControls(controls);
}


void Engine::wire() {
  world->attachRenderer(renderer);
  wm->attachWorld(world);
  wm->addAppsToWorld();
  //world->loadLatest();
  //world->loadMinecraft();
}

void Engine::loop() {
  vector<double> frameTimes(20, 0);
  double frameStart;
  int frameIndex = 0;
  double fps;
  try {
    while (!glfwWindowShouldClose(window)) {
      glfwPollEvents();

      engineGui->render(fps, frameIndex, frameTimes);
      frameStart = glfwGetTime();
      world->tick();
      renderer->render();
      api->mutateWorld();
      wm->mutateWorld();
      controls->poll(window, camera, world);

      frameTimes[frameIndex] = glfwGetTime() - frameStart;
      frameIndex = (frameIndex + 1) % 20;

      ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

      glfwSwapBuffers(window);
    }
  } catch (const std::exception &e) {
    logger->error(e.what());
    throw;
  }
}
