#include "engineGui.h"
#include "components/Door.h"
#include "components/Key.h"
#include "components/Lock.h"
#include "components/Parent.h"
#include "components/RotateMovement.h"
#include "glm/gtc/type_ptr.hpp"
#include "imgui/imgui.h"
#include "logger.h"
#include "persister.h"
#include "string"
#include <string>
#include "entity.h"
#include "systems/Door.h"
#include "systems/KeyAndLock.h"

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
  static int ROTATE_MOVEMENT_TYPE = 3;
  static int DOOR_TYPE = 4;
  static int KEY_TYPE = 5;
  static int LOCK_TYPE = 6;
  static int PARENT_TYPE = 7;
  static int selectedComponentType = LIGHT_TYPE; // Initialize

  ImGui::Combo("Component Type", &selectedComponentType,
               "Light\0Positionable\0Model\0RotateMovement\0Door\0Key\0Lock\0Parent\0");

  if (selectedComponentType == LIGHT_TYPE) {
    static glm::vec3 lightColor = glm::vec3(1.0f, 1.0f, 1.0f); // Default color

    ImGui::ColorEdit3("Color", (float *)&lightColor);

    if (ImGui::Button("Add Light Component")) {
      registry->emplace<Light>(entity, lightColor);
      showAddComponentPanel = false;
    }
  } else if (selectedComponentType == POSITIONABLE_TYPE) {
    static glm::vec3 position = glm::vec3(0.0f);
    static glm::vec3 origin = glm::vec3(0.0f);
    static glm::vec3 rotation = glm::vec3(0.0f);
    static float scale = 1.0f;

    ImGui::InputFloat3("Position", (float *)&position);
    ImGui::InputFloat3("Origin", (float *)&origin);
    ImGui::InputFloat3("Rotation", (float *)&rotation);
    ImGui::InputFloat("Scale", &scale);

    if (ImGui::Button("Add Positionable Component")) {
      registry->emplace<Positionable>(entity, position, origin, rotation, scale);
      showAddComponentPanel = false;
    }
  } else if (selectedComponentType == MODEL_TYPE) {
    static char modelPath[128] = "";
    ImGui::InputText("Model Path##New", modelPath, IM_ARRAYSIZE(modelPath));

    if (ImGui::Button("Add Model Component")) {
      registry->emplace<Model>(entity, modelPath);
      showAddComponentPanel = false;
    }
  } else if (selectedComponentType == ROTATE_MOVEMENT_TYPE) {
    static float degreesToRotate = 0;
    static float degreesPerSecond = 0;
    static glm::vec3 axis = glm::vec3(0.0f);
    ImGui::InputFloat("Degrees", &degreesToRotate);
    ImGui::InputFloat("Degrees/s", &degreesPerSecond);
    ImGui::InputFloat3("Rotation Axis", glm::value_ptr(axis));

    if (ImGui::Button("Add RotateMovement Component")) {
      registry->emplace<RotateMovement>
        (entity, degreesToRotate, degreesPerSecond, axis);
      showAddComponentPanel = false;
    }
  } else if(selectedComponentType == DOOR_TYPE) {
    static float openDegreesToRotate = 0;
    static float openDegreesPerSecond = 0;
    static float closeDegreesToRotate = 0;
    static float closeDegreesPerSecond = 0;
    static glm::vec3 openAxis = glm::vec3(0.0f);
    static glm::vec3 closeAxis = glm::vec3(0.0f);
    static DoorState doorState;

    ImGui::Text("Open RotateMovement");
    ImGui::InputFloat("Degrees##toOpen", &openDegreesToRotate);
    ImGui::InputFloat("Degrees/s##toOpen", &openDegreesPerSecond);
    ImGui::InputFloat3("Rotation Axis##toOpen", glm::value_ptr(openAxis));

    ImGui::Text("Closed RotateMovement");
    ImGui::InputFloat("Degrees##toClose", &closeDegreesToRotate);
    ImGui::InputFloat("Degrees/s##toClose", &closeDegreesPerSecond);
    ImGui::InputFloat3("Rotation Axis##toClose", glm::value_ptr(closeAxis));

    ImGui::RadioButton("Open",  (int*) &doorState, (int)DoorState::OPEN);
    ImGui::RadioButton("Closed", (int*) &doorState, (int)DoorState::CLOSED);

    if (ImGui::Button("Add Door Component")) {
      auto open = RotateMovement{openDegreesToRotate, openDegreesPerSecond, openAxis};
      auto close = RotateMovement{closeDegreesToRotate, closeDegreesPerSecond, closeAxis};
      registry->emplace<Door>(entity, open, close, doorState);
      showAddComponentPanel = false;
    }
  } else if(selectedComponentType == KEY_TYPE) {
    static float turnDegreesToRotate = 0;
    static float turnDegreesPerSecond = 0;
    static float unturnDegreesToRotate = 0;
    static float unturnDegreesPerSecond = 0;
    static glm::vec3 turnAxis = glm::vec3(0.0f);
    static glm::vec3 unturnAxis = glm::vec3(0.0f);
    static TurnState turnState;
    static int lockable = 0;

    ImGui::InputInt("Lockable entity ID", &lockable);

    ImGui::Text("Turn RotateMovement");
    ImGui::InputFloat("Degrees##toTurn", &turnDegreesToRotate);
    ImGui::InputFloat("Degrees/s##toTurn", &turnDegreesPerSecond);
    ImGui::InputFloat3("Rotation Axis##toTurn", glm::value_ptr(turnAxis));

    ImGui::Text("Unturnd RotateMovement");
    ImGui::InputFloat("Degrees##toUnturn", &unturnDegreesToRotate);
    ImGui::InputFloat("Degrees/s##toUnturn", &unturnDegreesPerSecond);
    ImGui::InputFloat3("Rotation Axis##toUnturn", glm::value_ptr(unturnAxis));

    ImGui::RadioButton("Turned",  (int*) &turnState, (int)TurnState::TURNED);
    ImGui::RadioButton("Unturned", (int*) &turnState, (int)TurnState::UNTURNED);

    if (ImGui::Button("Add Turn Component")) {
      auto turn = RotateMovement{turnDegreesToRotate, turnDegreesPerSecond, turnAxis};
      auto unturn = RotateMovement{unturnDegreesToRotate, unturnDegreesPerSecond, unturnAxis};
      registry->emplace<Key>(entity, lockable, turnState, turn, unturn);
      showAddComponentPanel = false;
    }
  } else if (selectedComponentType == LOCK_TYPE) {
    static glm::vec3 lockPosition = glm::vec3(0.0f);
    static glm::vec3 lockTolerance = glm::vec3(0.0f);
    static LockState lockState;

    ImGui::InputFloat3("Position", (float *)&lockPosition);
    ImGui::InputFloat3("Tolerance", (float *)&lockTolerance);

    ImGui::RadioButton("Locked", (int *)&lockState, (int)LockState::LOCKED);
    ImGui::RadioButton("Unlocked", (int *)&lockState, (int)LockState::UNLOCKED);

    if (ImGui::Button("Add Lock Component")) {
      registry->emplace<Lock>(entity, lockPosition, lockTolerance, lockState);
      showAddComponentPanel = false;
    }
  } else if(selectedComponentType == PARENT_TYPE) {
    static vector<int> childrenIds;
    for (int i = 0; i < childrenIds.size(); i++) {
      ImGui::InputInt(("Children ID##" + std::to_string(i)).c_str(),
                      &childrenIds[i]);
    }
    if (ImGui::Button("+ Add Another Child")) {
      childrenIds.push_back(0);
    }
    if (childrenIds.size() > 0 && ImGui::Button("+ Remove Child")) {
      childrenIds.pop_back();
    }
    if(ImGui::Button("Add Parent Component")) {
      registry->emplace<Parent>(entity, childrenIds);
      childrenIds = vector<int>();
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
    auto copiedOrigin = positionable.origin;
    auto copiedRotate = positionable.rotate;
    auto copiedScale = positionable.scale;

    ImGui::Text("Positioner Component:");
    ImGui::BeginGroup();
    ImGui::InputFloat3(("Position##" + to_string((int)entity)).c_str(),
                       (float *)&positionable.pos);
    ImGui::InputFloat3(("Origin##" + to_string((int)entity)).c_str(),
                       (float *)&positionable.origin);
    ImGui::InputFloat3(("Rotation##" + to_string((int)entity)).c_str(),
                       (float *)&positionable.rotate);
    ImGui::InputFloat(("Scale##" + to_string((int)entity)).c_str(),
                      &positionable.scale);
    if (ImGui::Button(
            ("Delete Component##Positioner" + to_string((int)entity)).c_str())) {
      registry->removePersistent<Positionable>(entity);
    }
    ImGui::EndGroup();
    ImGui::Spacing();

    if(copiedPos != positionable.pos || positionable.origin != copiedOrigin || copiedScale != positionable.scale || copiedRotate != positionable.rotate) {
      positionable.update();
    }
  }
  if(registry->any_of<Model>(entity)) {
    auto &model = registry->get<Model>(entity);
    char modelPath[128] = "";
    strcpy(modelPath, model.path.c_str());
    ImGui::Text("Model Component:");
    ImGui::BeginGroup();
    ImGui::InputText(("Model Path##" + to_string((int)entity)).c_str(), modelPath,
                     IM_ARRAYSIZE(modelPath));
    if (ImGui::Button(
            ("Delete Component##Model" + to_string((int)entity)).c_str())) {
      registry->removePersistent<Model>(entity);
    }
    ImGui::EndGroup();
    ImGui::Spacing();
  }
  if(registry->any_of<RotateMovement>(entity)) {
    // not a reference, because I'm just displaying text
    auto rotateMovement = registry->get<RotateMovement>(entity);
    ImGui::Text("RotateMovement Component");
    ImGui::Text("Degrees: %f", rotateMovement.degrees);
    ImGui::Text("Degrees/s: %f", rotateMovement.degreesPerSecond);
    auto axis = rotateMovement.axis;
    ImGui::Text("Axis: (%f, %f, %f)", axis.x, axis.y, axis.z);
    if (ImGui::Button(
            ("Delete Component##RotateMovement" + to_string((int)entity)).c_str())) {
      registry->remove<RotateMovement>(entity);
    }
    ImGui::Spacing();
  }
  if(registry->any_of<Door>(entity)) {
    auto &door = registry->get<Door>(entity);

    ImGui::Text("Door Component");
    ImGui::Text("Open RotateMovement");
    ImGui::InputDouble(("Degrees##toOpen" + to_string((int)entity)).c_str(),
                       &door.openMovement.degrees);
    ImGui::InputDouble(("Degrees/s##toOpen" + to_string((int)entity)).c_str(),
                       &door.openMovement.degreesPerSecond);
    ImGui::InputFloat3(("Rotation Axis##toOpen" + to_string((int)entity)).c_str(),
        glm::value_ptr(door.openMovement.axis));

    ImGui::Text("Close RotateMovement");
    ImGui::InputDouble(("Degrees##toClose" + to_string((int)entity)).c_str(),
                       &door.closeMovement.degrees);
    ImGui::InputDouble(("Degrees/s##toClose" + to_string((int)entity)).c_str(),
                       &door.closeMovement.degreesPerSecond);
    ImGui::InputFloat3(("Rotation Axis##toClose" + to_string((int)entity)).c_str(),
        glm::value_ptr(door.closeMovement.axis));

    ImGui::RadioButton(("Open" + to_string((int)entity)).c_str(), (int *)&door.state,
                       (int)DoorState::OPEN);
    ImGui::RadioButton(("Closed" + to_string((int)entity)).c_str(), (int *)&door.state,
                       (int)DoorState::CLOSED);

    if (ImGui::Button(("Open Door##" + to_string((int)entity)).c_str())) {
      systems::openDoor(registry, entity);
    }
    if (ImGui::Button(("Close Door##" + to_string((int)entity)).c_str())) {
      systems::closeDoor(registry, entity);
    }
    if (ImGui::Button(("Delete Component##Door" + to_string((int)entity))
                .c_str())) {registry->removePersistent<Door>(entity);
    }
    ImGui::Spacing();
  }
  if(registry->any_of<Key>(entity)) {
    auto &key = registry->get<Key>(entity);

    ImGui::Text("Key Component");
    ImGui::InputInt(("Lockable##" + to_string((int)entity)).c_str(), &key.lockable);
    ImGui::Text("Open RotateMovement##Key");
    ImGui::InputDouble(("Degrees##toTurn" + to_string((int)entity)).c_str(),
                       &key.turnMovement.degrees);
    ImGui::InputDouble(("Degrees/s##toTurn" + to_string((int)entity)).c_str(),
                       &key.turnMovement.degreesPerSecond);
    ImGui::InputFloat3(("Rotation Axis##toTurn" + to_string((int)entity)).c_str(),
        glm::value_ptr(key.turnMovement.axis));

    ImGui::Text("Close RotateMovement");
    ImGui::InputDouble(("Degrees##toUnturn" + to_string((int)entity)).c_str(),
                       &key.unturnMovement.degrees);
    ImGui::InputDouble(("Degrees/s##toUnturn" + to_string((int)entity)).c_str(),
                       &key.unturnMovement.degreesPerSecond);
    ImGui::InputFloat3(("Rotation Axis##toUnturn" + to_string((int)entity)).c_str(),
        glm::value_ptr(key.unturnMovement.axis));

    ImGui::RadioButton(("Turned##" + to_string((int)entity)).c_str(), (int *)&key.state,
                       (int)TurnState::TURNED);
    ImGui::RadioButton(("Unturned##" + to_string((int)entity)).c_str(), (int *)&key.state,
                       (int)TurnState::UNTURNED);

    if (ImGui::Button(("Turn Key##" + to_string((int)entity)).c_str())) {
      systems::turnKey(registry, entity);
    }
    if (ImGui::Button(("Unturn Key##" + to_string((int)entity)).c_str())) {
      systems::unturnKey(registry, entity);
    }
    if (ImGui::Button(("Delete Component##Key" + to_string((int)entity))
                .c_str())) {registry->removePersistent<Key>(entity);
    }
    ImGui::Spacing();
  }
  if(registry->any_of<Lock>(entity)) {
    auto &lock = registry->get<Lock>(entity);

    ImGui::Text("Positioner Component:");
    ImGui::BeginGroup();
    ImGui::InputFloat3(("Position##Lock" + to_string((int)entity)).c_str(),
                       (float *)&lock.position);
    ImGui::InputFloat3(("Tolerance##Lock" + to_string((int)entity)).c_str(),
                       (float *)&lock.tolerance);
    ImGui::RadioButton(("Locked##" + to_string((int)entity)).c_str(),
                       (int *)&lock.state, (int)LockState::LOCKED);
    ImGui::RadioButton(("Unlocked##" + to_string((int)entity)).c_str(),
                       (int *)&lock.state, (int)LockState::UNLOCKED);
    if (ImGui::Button(("Lock##" + to_string((int)entity)).c_str())) {
      //lock
    }
    if (ImGui::Button(("Unlock##" + to_string((int)entity)).c_str())) {
      //unlock
    }

    if (ImGui::Button(
            ("Delete Component##Positioner" + to_string((int)entity)).c_str())) {
      registry->removePersistent<Positionable>(entity);
    }
    ImGui::EndGroup();
    ImGui::Spacing();
  }
  if(registry->any_of<Parent>(entity)) {
    auto &parent = registry->get<Parent>(entity);
    ImGui::Text("Parent Component:");
    for(int i = 0; i < parent.childrenIds.size(); i++) {
      ImGui::InputInt(("Child Id##" + to_string(i) + to_string((int)entity)).c_str(),
                      &parent.childrenIds[i]);
    }
    if (ImGui::Button(("- Remove Child##" + to_string((int)entity)).c_str())) {
      parent.childrenIds.pop_back();
    }
    if (ImGui::Button(("+ Add Child##" + to_string((int)entity)).c_str())) {
      parent.childrenIds.push_back(0);
    }
  }
}

void EngineGui::renderEntities() {
  auto view = registry->view<Persistable>();
  // Hashmap Declaration
  static std::unordered_map<entt::entity, bool> componentOptionsState;
  for (auto [entity, persistable] : view.each()) {
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Text("Entity ID: %s", std::to_string(persistable.entityId).c_str());
    bool &showAddComponentPanel = componentOptionsState[entity];
    if (ImGui::Button("- Delete Entity")) {
      registry->depersist(entity);
    }
    if(!showAddComponentPanel) {
      if (ImGui::Button(("+ Add Component##" + to_string((int)entity)).c_str())) {
        showAddComponentPanel = true; // Show the options on button press
      }
    }
    if (showAddComponentPanel) { // Only display the combo and fields if active
      if((ImGui::Button(("Go Back##" + to_string((int)entity)).c_str()))) {
        showAddComponentPanel = false;
      }
      addComponentPanel(entity, showAddComponentPanel);
    }

    renderComponentPanel(entity);
  }
}

shared_ptr<LoggerVector> EngineGui::getLoggerVector() {
  return loggerVector;
}
