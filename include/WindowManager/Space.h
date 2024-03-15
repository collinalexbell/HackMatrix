#pragma once

#include <glm/glm.hpp>
#include <unordered_map>
#include <vector>
#include <queue>
#include "entity.h"
#include "loader.h"
#include "app.h"
#include <glm/gtx/hash.hpp>
#include <optional>

using namespace std;

class Renderer;
class Camera;

namespace WindowManager {
  class Space {
    shared_ptr<EntityRegistry> registry;
    shared_ptr<spdlog::logger> logger;
    Renderer *renderer = NULL;
    Camera *camera = NULL;
    queue<glm::vec3> availableAppPositions;

    size_t numPositionableApps = 0;

    void initAppPositions();

  public:
    Space(shared_ptr<EntityRegistry>, Renderer *, Camera *, spdlog::sink_ptr);

    float getViewDistanceForWindowSize(entt::entity);
    glm::vec3 getAppPosition(entt::entity);
    glm::vec3 getAppRotation(entt::entity);
    optional<entt::entity> getLookedAtApp();

    size_t getNumPositionableApps();

    void addApp(entt::entity, bool = false);
    void removeApp(entt::entity);
  };
}
