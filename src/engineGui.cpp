#include "engineGui.h"
#include "string"

EngineGui::EngineGui(GLFWwindow *window) {
  loggerVector = make_shared<LoggerVector>();

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

void EngineGui::render(double &fps, int frameIndex, vector<double> &frameTimes) {
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
  // ImGui::ShowDemoWindow();
  // ImGui::SetNextWindowSize(ImVec2(120, 5));
  if (frameIndex == 0) {
    fps = 0;
    for (int i = 0; i < 20; i++) {
      fps = fps + frameTimes[i];
    }
    fps /= 20.0;
    if (fps > 0)
      fps = 1.0 / fps;
  }
  ImGui::Begin("HackMatrix");
  vector<string> debugMessages = loggerVector->fetch();
  if (ImGui::BeginTabBar("Developer Menu")) {
    if (ImGui::BeginTabItem("FPS")) {
      ImGui::Text("%f fps", fps);
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Debug Log")) {
      for (const auto &msg : debugMessages) {
        ImGui::TextWrapped("%s", msg.c_str());
      }
      ImGui::EndTabItem();
    }
    ImGui::EndTabBar(); // Close the tab bar
  }
  ImGui::End();
  ImGui::Render();
}

shared_ptr<LoggerVector> EngineGui::getLoggerVector() {
  return loggerVector;
}
