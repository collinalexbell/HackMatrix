#include "WindowManager/Space.h"
#include "camera.h"
#include "renderer.h"
#include <glm/gtx/intersect.hpp>
#include <iterator>
#include <optional>

namespace WindowManager {

void Space::initAppPositions() {
  float z = 0.3;
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

Space::Space(Renderer *renderer, Camera *camera, spdlog::sink_ptr loggerSink)
    : renderer(renderer), camera(camera) {
  initAppPositions();
  logger = make_shared<spdlog::logger>("World", loggerSink);
  logger->set_level(spdlog::level::debug);
}

vector<X11App *> Space::getDirectRenderApps() {
  vector<X11App *> rv;
  for (auto app : directRenderApps) {
    rv.push_back(app.first);
  }
  return rv;
}

void Space::removeApp(X11App *app) {
  int index = -1;
  for (int i = 0; i < apps.size(); i++) {
    if (apps[i] == app) {
      index = i;
    }
  }

  if (index < 0) {
    return;
  }

  apps.erase(apps.begin() + index);
  appPositions.erase(appPositions.begin() + index);

  auto directRenderIt =
      std::find_if(directRenderApps.begin(), directRenderApps.end(),
                   [index](const std::pair<X11App *, int> &element) {
                     return element.second == index;
                   });

  if (directRenderIt != directRenderApps.end()) {
    directRenderApps.erase(directRenderIt);
    for (auto appKV = directRenderApps.begin(); appKV != directRenderApps.end();
         appKV++) {
      if (appKV->second > index) {
        appKV->second--;
      }
    }
  } else {
    numPositionableApps--;
  }

  renderer->deregisterApp(index);
  refreshRendererCubes();
}

void Space::refreshRendererCubes() {
  vector<glm::vec3> appCubesV;
  for(auto position: appPositions) {
    if(position.has_value()) {
      appCubesV.push_back(position.value());
    }
  }
  for (int i = 0; i < appCubesV.size(); i++) {
    renderer->addAppCube(i, appCubesV[i]);
  }
}

float Space::getViewDistanceForWindowSize(X11App *app) {
  // view = projection^-1 * gl_vertex * vertex^-1
  float glVertexX = float(app->width) / 1920;
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

glm::vec3 Space::getAppPosition(X11App *app) {
  int index = -1;
  for (int i = 0; i < apps.size(); i++) {
    if (app == apps[i]) {
      index = i;
    }
  }
  if (index == -1) {
    throw "app not found";
  }

  auto appPosition = appPositions[index];
  if(!appPosition.has_value()) {
    throw "app is not positionable";
  }

  return appPosition.value();
}

struct Intersection {
  glm::vec3 intersectionPoint;
  float dist;
};

Intersection intersectLineAndPlane(glm::vec3 linePos, glm::vec3 lineDir,
                                   glm::vec3 planePos) {
  Intersection intersection;
  glm::vec3 normLineDir = glm::normalize(lineDir);
  glm::intersectRayPlane(linePos, normLineDir, planePos, glm::vec3(0, 0, 1),
                         intersection.dist);
  intersection.intersectionPoint = (normLineDir * intersection.dist) + linePos;
  return intersection;
}

X11App *Space::getLookedAtApp() {
  float DIST_LIMIT = 1.5;
  float height = 0.74;
  float width = 1.0;
  for (int index = 0; index < appPositions.size(); index++) {
    auto appPositionOptional = appPositions[index];
    if(appPositionOptional.has_value()) {
      auto appPosition = appPositionOptional.value();
      Intersection intersection =
          intersectLineAndPlane(camera->position, camera->front, appPosition);
      float minX = appPosition.x - (width / 3);
      float maxX = appPosition.x + (width / 3);
      float minY = appPosition.y - (height / 3);
      float maxY = appPosition.y + (height / 3);
      float x = intersection.intersectionPoint.x;
      float y = intersection.intersectionPoint.y;
      if (x > minX && x < maxX && y > minY && y < maxY &&
          intersection.dist < DIST_LIMIT) {
        X11App *app = apps[index];
        return app;
      }
    }
  }
  return NULL;
}

int Space::getIndexOfApp(X11App *app) {
  for (int i = 0; i < apps.size(); i++) {
    if (app == apps[i]) {
      return i;
    }
  }
  return -1;
}

size_t Space::getNumPositionableApps() {
  return numPositionableApps;
}

void Space::addApp(X11App *app) {
  if (!app->isAccessory()) {
    glm::vec3 pos = availableAppPositions.front();
    numPositionableApps++;
    addApp(pos, app);
    if (availableAppPositions.size() > 1) {
      availableAppPositions.pop();
    }
  } else {
    if (app->width > 30) {
      stringstream ss;
      ss << "accessory app of size: " << app->width << "x" << app->height;
      logger->debug(ss.str());
      int index = apps.size();
      directRenderApps.push_back(make_pair(app, index));
      apps.push_back(app);
      appPositions.push_back(std::nullopt);
      try {
        renderer->registerApp(app, index);
      } catch (...) {
        logger->info("accessory app failed to register texture");
        apps.pop_back();
        directRenderApps.pop_back();
      }
    }
  }
}

void Space::addApp(glm::vec3 pos, X11App *app) {
  int index = apps.size();
  apps.push_back(app);
  appPositions.push_back(pos);
  if (renderer != NULL) {
    renderer->registerApp(app, index);
    renderer->addAppCube(index, pos);
  }
}

} // namespace WindowManager
