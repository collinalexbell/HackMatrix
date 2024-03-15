#pragma once

#include <glm/glm.hpp>
#include <unordered_map>
#include <vector>
#include <queue>
#include "loader.h"
#include "app.h"
#include <glm/gtx/hash.hpp>
#include <optional>

using namespace std;

class Renderer;
class Camera;

namespace WindowManager {
  class Space {
    shared_ptr<spdlog::logger> logger;
    Renderer *renderer = NULL;
    Camera *camera = NULL;
    vector<X11App *> apps;
    vector<optional<glm::vec3>> appPositions;
    vector<pair<X11App *, int>> directRenderApps;
    queue<glm::vec3> availableAppPositions;


    void initAppPositions();
  public:
    Space(Renderer *, Camera *, spdlog::sink_ptr);
    vector<X11App *> getDirectRenderApps();
    void removeApp(X11App *app);
    void refreshRendererCubes();
    float getViewDistanceForWindowSize(X11App *app);
    glm::vec3 getAppPosition(X11App *app);
    X11App *getLookedAtApp();
    int getIndexOfApp(X11App *app);
    void addApp(glm::vec3, X11App *app);
    void addApp(X11App *app);
    size_t numPositionableApps();
  };
}
