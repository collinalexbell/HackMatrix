#include "engineGui.h"
#include "imgui/imgui.h"
#include "logger.h"
#include "persister.h"
#include "string"
#include <string>
#include "entity.h"

#include <glm/glm.hpp>

EngineGui::EngineGui(GLFWwindow *window, shared_ptr<EntityRegistry> registry,
                     shared_ptr<LoggerVector> loggerVector)
    : registry(registry), loggerVector(loggerVector) {

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
    if (ImGui::BeginTabItem("Entity Editor")) {
      if (ImGui::Button("+ Create Entity")) {
        createNewEntity(); // Function to create an entity
      }
      ImGui::SameLine();
      if (ImGui::Button("Persist All")) {
        registry->saveAll();
      }
      renderEntities();
      ImGui::EndTabItem();
    }
    ImGui::EndTabBar(); // Close the tab bar
  }
  ImGui::End();
  ImGui::Render();
}



void EngineGui::createNewEntity() {
  entt::entity newEntity = registry->createPersistent();
}

void EngineGui::addComponentPanel(entt::entity entity,
                                  bool &showAddComponentPanel) {
  static int LIGHT_TYPE = 0;
  static int POSITIONABLE_TYPE = 1; // Adjusted indexes
  static int MODEL_TYPE = 2;
  static int selectedComponentType = LIGHT_TYPE; // Initialize

  ImGui::Combo("Component Type", &selectedComponentType,
               "Light\0Positionable\0Model\0");

  if (selectedComponentType == LIGHT_TYPE) {
    static glm::vec3 lightColor = glm::vec3(1.0f, 1.0f, 1.0f); // Default color

    ImGui::ColorEdit3("Color", (float *)&lightColor);

    if (ImGui::Button("Add Light Component")) {
      registry->emplace<Light>(entity, lightColor);
      showAddComponentPanel = false;
    }
  } else if (selectedComponentType == POSITIONABLE_TYPE) {
    static glm::vec3 position = glm::vec3(0.0f);
    static float scale = 1.0f;

    ImGui::InputFloat3("Position", (float *)&position);
    ImGui::InputFloat("Scale", &scale);

    if (ImGui::Button("Add Positionable Component")) {
      registry->emplace<Positionable>(entity, position, scale);
      showAddComponentPanel = false;
    }
  } else if (selectedComponentType == MODEL_TYPE) {
    static char modelPath[128] = "";
    ImGui::InputText("Model Path", modelPath, IM_ARRAYSIZE(modelPath));

    if (ImGui::Button("Add Model Component")) {
      registry->emplace<Model>(entity, modelPath);
      showAddComponentPanel = false;
    }
  }
}

void EngineGui::renderComponentPanel(entt::entity entity) {
  if(registry->any_of<Light>(entity)) {
    auto &light = registry->get<Light>(entity);
    ImGui::Text("Light Component:");
    ImGui::BeginGroup();
    ImGui::ColorEdit3(("Color##" + to_string((int)entity)).c_str(),
                      (float *)&light.color);
    if(ImGui::Button(("Delete Component##Light" + to_string((int)entity)).c_str())) {
      registry->removePersistent<Light>(entity);
    }
    ImGui::EndGroup();
    ImGui::Spacing();
  }
  if(registry->any_of<Positionable>(entity)) {
    auto &positionable = registry->get<Positionable>(entity);

    auto copiedPos = positionable.pos;
    auto copiedScale = positionable.scale;

    ImGui::Text("Positioner Component:");
    ImGui::BeginGroup();
    ImGui::InputFloat3(("Position##" + to_string((int)entity)).c_str(),
                       (float *)&positionable.pos);
    ImGui::InputFloat(("Scale##" + to_string((int)entity)).c_str(),
                      &positionable.scale);
    if (ImGui::Button(
            ("Delete Component##Positioner" + to_string((int)entity)).c_str())) {
      registry->removePersistent<Positionable>(entity);
    }
    ImGui::EndGroup();
    ImGui::Spacing();

    if(copiedPos != positionable.pos || copiedScale != positionable.scale) {
      positionable.update();
    }
  }
  if(registry->any_of<Model>(entity)) {
    auto &model = registry->get<Model>(entity);
    char modelPath[128] = "";
    strcpy(modelPath, model.path.c_str());
    ImGui::Text("Model Component:");
    ImGui::BeginGroup();
    ImGui::InputText("Model Path", modelPath, IM_ARRAYSIZE(modelPath));
    if (ImGui::Button(
            ("Delete Component##Model" + to_string((int)entity)).c_str())) {
      registry->removePersistent<Model>(entity);
    }
    ImGui::EndGroup();
    ImGui::Spacing();
  }
}

void EngineGui::renderEntities() {
  auto view = registry->view<Persistable>();
  // Hashmap Declaration
  static std::unordered_map<entt::entity, bool> componentOptionsState;
  for (auto [entity, persistable] : view.each()) {
    ImGui::Text("Entity ID: %s", std::to_string(persistable.entityId).c_str());
    bool &showAddComponentPanel = componentOptionsState[entity];
    if (ImGui::Button("- Delete Entity")) {
      registry->depersist(entity);
    }
    if (ImGui::Button("+ Add Component")) {
      showAddComponentPanel = true; // Show the options on button press
    }

    if (showAddComponentPanel) { // Only display the combo and fields if active
      addComponentPanel(entity, showAddComponentPanel);
    }

    renderComponentPanel(entity);
  }
}

shared_ptr<LoggerVector> EngineGui::getLoggerVector() {
  return loggerVector;
}
