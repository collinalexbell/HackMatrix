#include <spdlog/logger.h>
#include "WindowManager/Space.h"
#include "app.h"
#include "components/Bootable.h"
#include "camera.h"
#include "entity.h"
#include "glm/ext/quaternion_trigonometric.hpp"
#include "glm/gtc/quaternion.hpp"
#include "model.h"
#include "screen.h"
#include "renderer.h"
#include <glm/gtx/intersect.hpp>
#include <iterator>
#include <optional>

namespace WindowManager {

void Space::initAppPositions() {
  float z = -2;
  float xOffset = -1.2;
  float yOffset = -2.4;
  availableAppPositions.push(glm::vec3(0.0 + xOffset, 3.5 + yOffset, z));
  availableAppPositions.push(glm::vec3(1.2 + xOffset, 3.5 + yOffset, z));
  availableAppPositions.push(glm::vec3(1.2 + xOffset, 4.25 + yOffset, z));
  availableAppPositions.push(glm::vec3(1.2 + xOffset, 5.00 + yOffset, z));
  availableAppPositions.push(glm::vec3(2.4 + xOffset, 3.5 + yOffset, z));
  availableAppPositions.push(glm::vec3(0 + xOffset, 4.25 + yOffset, z));
  availableAppPositions.push(glm::vec3(2.4 + xOffset, 4.25 + yOffset, z));
  availableAppPositions.push(glm::vec3(2.4 + xOffset, 7 + yOffset, z));
}

  Space::Space(shared_ptr<EntityRegistry> registry, Renderer *renderer,
               Camera *camera, spdlog::sink_ptr loggerSink)
    : renderer(renderer), camera(camera), registry(registry) {
  initAppPositions();
  logger = make_shared<spdlog::logger>("WindowManager::Space", loggerSink);
  logger->set_level(spdlog::level::debug);
}

  void Space::removeApp(entt::entity entity) {
  if(registry->all_of<Positionable>(entity)) {
    numPositionableApps--;
  }

  auto &app = registry->get<X11App>(entity);
  renderer->deregisterApp(app.getAppIndex());
  registry->remove<X11App>(entity);
  //registry->destroy(entity);
}

float Space::getViewDistanceForWindowSize(entt::entity entity) {
  auto &app = registry->get<X11App>(entity);
  auto positionable = registry->try_get<Positionable>(entity);
  // view = projection^-1 * gl_vertex * vertex^-1
  float scaleFactor = positionable != NULL ? positionable->scale : 1;
  float glVertexX = float(app.width) / SCREEN_WIDTH / scaleFactor;
  glm::vec4 gl_pos = glm::vec4(10000, 0, 0, 0);
  float zBest;
  float target = glVertexX;
  for (float z = 0.0; z <= 10.5; z = z + 0.001) {
    glm::vec4 candidate;
    candidate = renderer->projection * glm::vec4(0.5, 0, -z, 1);
    candidate = candidate / candidate.w;
    if (abs(candidate.x - target) < abs(gl_pos.x - target)) {
      gl_pos = candidate;
      zBest = z;
    }
  }
  return zBest;
}

glm::vec3 Space::getAppPosition(entt::entity entity) {
  auto positionable = registry->try_get<Positionable>(entity);
  if(positionable == NULL) {
    throw "app doesn't exist or has no position";
  }
  return positionable->pos;
}

glm::vec3 Space::getAppRotation(entt::entity entity) {
  auto positionable = registry->try_get<Positionable>(entity);
  if (positionable == NULL) {
    throw "app doesn't exist or has no position";
  }
  return positionable->rotate;
}

struct Intersection {
  glm::vec3 intersectionPoint;
  float dist;
};

Intersection intersectLineAndPlane(glm::vec3 linePos, glm::vec3 lineDir,
                                   glm::vec3 planePos, glm::vec3 planeDir) {
  Intersection intersection;
  glm::vec3 normLineDir = glm::normalize(lineDir);
  glm::intersectRayPlane(linePos, normLineDir, planePos, planeDir,
                         intersection.dist);
  intersection.intersectionPoint = (normLineDir * intersection.dist) + linePos;
  return intersection;
}

optional<entt::entity> Space::getLookedAtApp() {
  float DIST_LIMIT = 1.5;
  float height = 0.74;
  float width = 1.0;
  auto view = registry->view<X11App, Positionable>();
  for (auto [entity, app, positionable]: view.each()) {
    auto appPosition = positionable.pos;

    glm::quat rotation = glm::quat(glm::radians(positionable.rotate));
    auto appDir = rotation * glm::vec3(0.0f,0.0f,1.0f);
    Intersection intersection =
      intersectLineAndPlane(camera->position, camera->front, appPosition, appDir);
    float minX = appPosition.x - (width / 3);
    float maxX = appPosition.x + (width / 3);
    float minY = appPosition.y - (height / 3);
    float maxY = appPosition.y + (height / 3);
    float x = intersection.intersectionPoint.x;
    float y = intersection.intersectionPoint.y;
    if (x > minX && x < maxX && y > minY && y < maxY &&
        intersection.dist < DIST_LIMIT) {
      return entity;
    }
  }
  return std::nullopt;
}

size_t Space::getNumPositionableApps() {
  return numPositionableApps;
}

void Space::addApp(entt::entity entity, bool spawnAtCamera) {
  try {
    auto &app = registry->get<X11App>(entity);
    auto hasPositionable = registry->all_of<Positionable>(entity);
    auto bootable = registry->try_get<Bootable>(entity);
    if (!app.isAccessory() && !hasPositionable && !bootable) {

      glm::vec3 pos;
      glm::vec3 rot = glm::vec3(0.0f);
      if(spawnAtCamera) {

        float yaw = camera->getYaw();
        float pitch = camera->getPitch();
        glm::quat yawRotation =
          glm::angleAxis(glm::radians(90+yaw), glm::vec3(0.0f, -1.0f, 0.0f));
        glm::quat pitchRotation =
          glm::angleAxis(glm::radians(pitch), glm::vec3(1.0f, 0.0f, 0.0f));
        glm::quat finalRotation = yawRotation * pitchRotation;
        rot = glm::degrees(glm::eulerAngles(finalRotation));

        auto dist = getViewDistanceForWindowSize(entity);
        pos = camera->position +
          finalRotation * glm::vec3(0,0,-dist);

      } else {
        pos = availableAppPositions.front();
        if (availableAppPositions.size() > 1) {
          availableAppPositions.pop();
        }
      }

      int index = numPositionableApps++;
      registry->emplace<Positionable>(entity, pos, glm::vec3(0.0), rot, 1);
    }
    if (!app.isAccessory() && bootable) {
      systems::resizeBootable(registry, entity, bootable->getWidth(),
          bootable->getHeight());
    }
    try {
      renderer->registerApp(&app);
      auto positionable = registry->try_get<Positionable>(entity);
      if(positionable){
        positionable->damage();
      }
    } catch (...) {
      logger->info("accessory app failed to register texture");
      registry->remove<X11App>(entity);
    }
  } catch(...) {}
}
} // namespace WindowManager
