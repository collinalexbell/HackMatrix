#pragma once

#include <glm/glm.hpp>
#include <unordered_map>
#include <vector>
#include <queue>
#include "entity.h"
#include "loader.h"
#include <glm/gtx/hash.hpp>
#include <optional>

using namespace std;

class Renderer;
class Camera;
struct Ray;

namespace WindowManager {
  struct AppRayHit {
    entt::entity entity = entt::null;
    glm::vec3 worldPoint = glm::vec3(0.0f);
    glm::vec2 surfacePixels = glm::vec2(0.0f);
    float distance = 0.0f;
  };

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
    optional<AppRayHit> raycastApp(const Ray& ray,
                                   float distLimit = 20.0f);
    optional<AppRayHit> raycastAppFromScreen(float mouseX,
                                             float mouseY,
                                             float screenWidth,
                                             float screenHeight,
                                             float distLimit = 20.0f);

    size_t getNumPositionableApps();

    void removeApp(entt::entity);
  };
}
