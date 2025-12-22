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
#include "wayland_app.h"
#include "components/Bootable.h"
#include <glm/gtx/intersect.hpp>
#include <cassert>
#include <csignal>
#include <cstdio>
#include <cmath>
#include <iterator>
#include <optional>

namespace WindowManager {

static void
debugBreak(const char* msg)
{
  FILE* f = std::fopen("/tmp/matrix-debug.log", "a");
  if (f) {
  std::fprintf(f, "DEBUG_BREAK: %s\n", msg ? msg : "(null)");
  std::fflush(f);
  std::fclose(f);
}
  std::raise(SIGTRAP);
}

void
Space::initAppPositions()
{
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

Space::Space(shared_ptr<EntityRegistry> registry,
             Renderer* renderer,
             Camera* camera,
             spdlog::sink_ptr loggerSink)
  : renderer(renderer)
  , camera(camera)
  , registry(registry)
{
  initAppPositions();
  logger = make_shared<spdlog::logger>("WindowManager::Space", loggerSink);
  logger->set_level(spdlog::level::debug);
}

void
Space::removeApp(entt::entity entity)
{
  if (registry->all_of<Positionable>(entity)) {
    numPositionableApps--;
  }

  AppSurface* surface = nullptr;
  if (registry->all_of<X11App>(entity)) {
    surface = &registry->get<X11App>(entity);
    registry->remove<X11App>(entity);
  } else if (registry->all_of<WaylandApp::Component>(entity)) {
    auto& comp = registry->get<WaylandApp::Component>(entity);
    surface = comp.app.get();
    registry->remove<WaylandApp::Component>(entity);
  }
  if (surface) {
    renderer->deregisterApp(static_cast<int>(surface->getAppIndex()));
  }
  // registry->destroy(entity);
}

float
Space::getViewDistanceForWindowSize(entt::entity entity)
{
  AppSurface* surface = nullptr;
  if (registry->all_of<X11App>(entity)) {
    surface = &registry->get<X11App>(entity);
  } else if (registry->all_of<WaylandApp::Component>(entity)) {
    auto& comp = registry->get<WaylandApp::Component>(entity);
    surface = comp.app.get();
  }
  if (!surface) {
    return 1.0f;
  }
  auto positionable = registry->try_get<Positionable>(entity);
  // view = projection^-1 * gl_vertex * vertex^-1
  float scaleFactor = positionable != NULL ? positionable->scale : 1;
  float targetWidthPx = surface->getWidth();
  // Use the cached screen width (updated by the renderer from the GL viewport)
  // so Wayland buffer sizes map correctly to screen pixels.
  float viewportWidthPx = SCREEN_WIDTH;
  auto log_state = [&](const char* tag,
                       float zVal,
                       float ndcHalf = 0.0f,
                       float desiredHalf = 0.0f,
                       float diff = 0.0f,
                       float expectedCov = -1.0f,
                       float p00Val = 0.0f,
                       float tanVal = 0.0f) {
    FILE* f = std::fopen("/tmp/matrix-debug.log", "a");
    if (!f) {
      return;
    }
    float expected =
      expectedCov >= 0.0f
        ? expectedCov
        : (viewportWidthPx > 0.0f ? targetWidthPx / viewportWidthPx : 0.0f);
    std::fprintf(
      f,
      "Space::state %s ent=%d width=%.2f viewportW=%.2f screen=(%.2f,%.2f) "
      "defaults=(%d,%d) scale=%.6f target=%.6f expectedCoverage=%.6f p00=%.6f "
      "tanHalfXFov=%.6f z=%.6f ndcHalf=%.6f desiredHalf=%.6f diff=%.6f\n",
      tag ? tag : "(null)",
      (int)entt::to_integral(entity),
      targetWidthPx,
      viewportWidthPx,
      SCREEN_WIDTH,
      SCREEN_HEIGHT,
      Bootable::DEFAULT_WIDTH,
      Bootable::DEFAULT_HEIGHT,
      scaleFactor,
      targetWidthPx / (viewportWidthPx > 0.0f ? viewportWidthPx : 1.0f) /
        scaleFactor,
      expected,
      p00Val,
      tanVal,
      zVal,
      ndcHalf,
      desiredHalf,
      diff);
    std::fflush(f);
    std::fclose(f);
  };

  if (!(viewportWidthPx > 0.0f)) {
    viewportWidthPx = 1280.0f; // safe fallback to keep running
    log_state("missing viewport width", 0.0f);
    debugBreak("Space::getViewDistanceForWindowSize missing viewport width");
    assert(viewportWidthPx > 0.0f && "Viewport width must be > 0");
  }
  float target = targetWidthPx / viewportWidthPx / scaleFactor;
  if (!std::isfinite(target)) {
    log_state("invalid target", 0.0f);
    debugBreak("Space::getViewDistanceForWindowSize invalid target");
    assert(std::isfinite(target) && "Target must be finite");
  }

  // Analytically solve for z so the quad width maps 1:1 to pixels:
  // x_ndc = (0.5 * P00) / z, target = |x_ndc| => z = 0.5 * P00 / target
  glm::mat4 proj = camera->getProjectionMatrix();
  float p00 = proj[0][0];
  float tanHalfXFov = p00 != 0.0f ? 1.0f / p00 : 0.0f;
  float expectedCoverage = targetWidthPx / viewportWidthPx;
  if (target > 1e-4f && tanHalfXFov > 0.0f) {
    float z = 0.5f * scaleFactor / (target * tanHalfXFov);
    if (!std::isfinite(z) || z <= 0.0f) {
      log_state("analytic z invalid", z);
      debugBreak("Space::getViewDistanceForWindowSize analytic z invalid");
      assert(std::isfinite(z) && z > 0.0f && "Solved z must be finite and > 0");
    }
    float ndcHalf =
      fabs(p00 * 0.5f * scaleFactor / z); // projected half-width in NDC
    float desiredHalf = expectedCoverage * 0.5f;
    float diff = fabs(ndcHalf - desiredHalf);
    bool bad = desiredHalf > 1e-4f ? (diff / desiredHalf) > 0.05f : diff > 1e-4f;
    if (bad) {
      log_state(
        "width mismatch", z, ndcHalf, desiredHalf, diff, expectedCoverage, p00, tanHalfXFov);
    }
    log_state(
      "analytic ok", z, ndcHalf, desiredHalf, diff, expectedCoverage, p00, tanHalfXFov);
    FILE* f = std::fopen("/tmp/matrix-debug.log", "a");
    if (f) {
      std::fprintf(f,
                   "Space::analytic ent=%d width=%.2f viewportW=%.2f scale=%.4f "
                   "target=%.6f tanHalfXFov=%.6f z=%.6f ndcHalf=%.6f desiredHalf=%.6f diff=%.6f\n",
                   (int)entt::to_integral(entity),
                   targetWidthPx,
                   viewportWidthPx,
                   scaleFactor,
                   target,
                   tanHalfXFov,
                   z,
                   ndcHalf,
                   desiredHalf,
                   diff);
      std::fflush(f);
      std::fclose(f);
    }
    return z;
  }

  // Fallback to the previous search if projection entries are degenerate.
  glm::vec4 gl_pos = glm::vec4(10000, 0, 0, 0);
  float zBest = 1.0f;
  for (float z = 0.0f; z <= 10.5f; z += 0.001f) {
    glm::vec4 candidate =
      camera->getProjectionMatrix() *
      glm::vec4(0.5f * scaleFactor, 0.0f, -z, 1.0f);
    candidate = candidate / candidate.w;
    if (fabs(candidate.x - target) < fabs(gl_pos.x - target)) {
      gl_pos = candidate;
      zBest = z;
    }
  }
  FILE* f = std::fopen("/tmp/matrix-debug.log", "a");
  if (f) {
    float ndcHalf = fabs(gl_pos.x);
    float desiredHalf = expectedCoverage * 0.5f;
    float diff = ndcHalf - desiredHalf;
    bool bad = desiredHalf > 1e-4f ? (fabs(diff) / desiredHalf) > 0.05f
                                   : fabs(diff) > 1e-4f;
    if (bad) {
      log_state("fallback mismatch",
                zBest,
                ndcHalf,
                desiredHalf,
                diff,
                expectedCoverage,
                p00,
                tanHalfXFov);
    }
    std::fprintf(f,
                 "Space::fallback ent=%d width=%.2f viewportW=%.2f scale=%.4f "
                 "target=%.6f tanHalfXFov=%.6f zBest=%.6f ndcHalf=%.6f desiredHalf=%.6f diff=%.6f\n",
                 (int)entt::to_integral(entity),
                 targetWidthPx,
                 viewportWidthPx,
                 scaleFactor,
                 target,
                 tanHalfXFov,
                 zBest,
                 ndcHalf,
                 desiredHalf,
                 diff);
    std::fflush(f);
    std::fclose(f);
  }
  return zBest;
}

glm::vec3
Space::getAppPosition(entt::entity entity)
{
  auto positionable = registry->try_get<Positionable>(entity);
  if (positionable == NULL) {
    throw "app doesn't exist or has no position";
  }
  return positionable->pos;
}

glm::vec3
Space::getAppRotation(entt::entity entity)
{
  auto positionable = registry->try_get<Positionable>(entity);
  if (positionable == NULL) {
    throw "app doesn't exist or has no position";
  }
  return positionable->rotate;
}

struct Intersection
{
  glm::vec3 intersectionPoint;
  float dist;
};

Intersection
intersectLineAndPlane(glm::vec3 linePos,
                      glm::vec3 lineDir,
                      glm::vec3 planePos,
                      glm::vec3 planeDir)
{
  Intersection intersection;
  glm::vec3 normLineDir = glm::normalize(lineDir);
  glm::intersectRayPlane(
    linePos, normLineDir, planePos, planeDir, intersection.dist);
  intersection.intersectionPoint = (normLineDir * intersection.dist) + linePos;
  return intersection;
}

optional<entt::entity>
Space::getLookedAtApp()
{
  float DIST_LIMIT = 2.5;
  float height = 0.74;
  float width = 1.0;
  auto checkHit = [&](entt::entity entity,
                      Positionable& positionable) -> optional<entt::entity> {
    auto appPosition = positionable.pos;
    glm::quat rotation = glm::quat(glm::radians(positionable.rotate));
    auto appDir = rotation * glm::vec3(0.0f, 0.0f, 1.0f);
    Intersection intersection = intersectLineAndPlane(
      camera->position, camera->front, appPosition, appDir);
    float minX = appPosition.x - (width / 3);
    float maxX = appPosition.x + (width / 3);
    float minY = appPosition.y - (height / 3);
    float maxY = appPosition.y + (height / 3);
    float x = intersection.intersectionPoint.x;
    float y = intersection.intersectionPoint.y;
    if (x > minX && x < maxX && y > minY && y < maxY &&
        intersection.dist < DIST_LIMIT && intersection.dist > 0.0) {
      return entity;
    }
    return std::nullopt;
  };

  auto view = registry->view<X11App, Positionable>();
  for (auto [entity, app, positionable] : view.each()) {
    if (auto hit = checkHit(entity, positionable)) {
      return hit;
    }
  }
  auto wlView = registry->view<WaylandApp::Component, Positionable>();
  for (auto [entity, comp, positionable] : wlView.each()) {
    if (!comp.app) {
      continue;
    }
    if (auto hit = checkHit(entity, positionable)) {
      return hit;
    }
  }
  return std::nullopt;
}

size_t
Space::getNumPositionableApps()
{
  return numPositionableApps;
}

void
Space::addApp(entt::entity entity, bool spawnAtCamera)
{
  try {
    auto& app = registry->get<X11App>(entity);
    auto hasPositionable = registry->all_of<Positionable>(entity);
    auto bootable = registry->try_get<Bootable>(entity);
    if (!app.isAccessory() && !hasPositionable && !bootable) {

      glm::vec3 pos;
      glm::vec3 rot = glm::vec3(0.0f);
      if (spawnAtCamera) {

        float yaw = camera->getYaw();
        float pitch = camera->getPitch();
        glm::quat yawRotation =
          glm::angleAxis(glm::radians(90 + yaw), glm::vec3(0.0f, -1.0f, 0.0f));
        glm::quat pitchRotation =
          glm::angleAxis(glm::radians(pitch), glm::vec3(1.0f, 0.0f, 0.0f));
        glm::quat finalRotation = yawRotation * pitchRotation;
        rot = glm::degrees(glm::eulerAngles(finalRotation));

        auto dist = getViewDistanceForWindowSize(entity);
        pos = camera->position + finalRotation * glm::vec3(0, 0, -dist);

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
      systems::resizeBootable(
        registry, entity, bootable->getWidth(), bootable->getHeight());
    }
    try {
      renderer->registerApp(&app);
      auto positionable = registry->try_get<Positionable>(entity);
      if (positionable) {
        positionable->damage();
      }
    } catch (...) {
      logger->info("accessory app failed to register texture");
      registry->remove<X11App>(entity);
    }
  } catch (...) {
  }
}

void
Space::toggleAppSelect(entt::entity appEntt)
{
  auto app = registry->try_get<X11App>(appEntt);
  if (app) {
    std::stringstream ss;
    ss << "selecting app: " << (int)appEntt << endl;
    logger->info("selecting app");
    if (app->isSelected()) {
      app->deselect();
    } else {
      app->select();
    }

  } else {
    logger->error("attempted to select a non existent app");
  }
}
} // namespace WindowManager
