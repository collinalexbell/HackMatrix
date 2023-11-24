#define GLFW_EXPOSE_NATIVE_X11
#include "engine.h"
#include <GLFW/glfw3native.h>
#include <GLFW/glfw3.h>

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"

void mouseCallback(GLFWwindow *window, double xpos, double ypos) {
  Engine *engine = (Engine *)glfwGetWindowUserPointer(window);
  engine->controls->mouseCallback(window, xpos, ypos);
}

void Engine::registerCursorCallback() {
  glfwSetWindowUserPointer(window, (void *)this);
  glfwSetCursorPosCallback(window, mouseCallback);
}

Engine::Engine(GLFWwindow* window, char** envp): window(window) {
  initialize();
  logger = make_shared<spdlog::logger>("engine", fileSink);
  wm->createAndRegisterApps(envp);
  glfwFocusWindow(window);
  wire();
  registerCursorCallback();

  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  io.ConfigFlags |=
      ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
  io.ConfigFlags |=
      ImGuiConfigFlags_NavEnableGamepad; // Enable Gamepad Controls

  // Setup Platform/Renderer backends
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init();
}

Engine::~Engine() {
  delete controls;
  delete renderer;
  delete world;
  delete camera;
  //delete wm;
  delete api;
}

void Engine::initialize(){
  wm = new WM(glfwGetX11Window(window));
  camera = new Camera();
  world = new World(camera, true);
  api = new Api("tcp://*:3333", world);
  renderer = new Renderer(camera, world);
  controls = new Controls(wm, world, camera, renderer);
  wm->registerControls(controls);
}


void Engine::wire() {
  world->attachRenderer(renderer);
  wm->attachWorld(world);
  wm->addAppsToWorld();
  world->loadLatest();
}

void Engine::loop() {
  vector<double> frameTimes(20, 0);
  double frameStart;
  int frameIndex = 0;
  double fps;
  try {
    while (!glfwWindowShouldClose(window)) {
      frameStart = glfwGetTime();
      glfwPollEvents();

      ImGui_ImplOpenGL3_NewFrame();
      ImGui_ImplGlfw_NewFrame();
      ImGui::NewFrame();
      //ImGui::ShowDemoWindow();
      //ImGui::SetNextWindowSize(ImVec2(120, 5));
      if(frameIndex == 0) {
        fps = 0;
        for(int i=0; i<20; i++) {
          fps = fps + frameTimes[i];
        }
        fps /= 20.0;
        if(fps > 0) fps = 1.0/fps;
      }
      ImGui::Begin("HackMatrix");
      ImGui::Text("%f fps", fps);
      ImGui::End();
      ImGui::Render();

      renderer->render();
      api->mutateWorld();
      wm->mutateWorld();
      controls->poll(window, camera, world);

      ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

      glfwSwapBuffers(window);
      frameTimes[frameIndex] = glfwGetTime() - frameStart;
      frameIndex = (frameIndex + 1)%20;
    }
  } catch (const std::exception &e) {
    logger->error(e.what());
    throw;
  }
}

