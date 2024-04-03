#pragma once

#include "SQLPersisterImpl.h"
#include <functional>
#include "glm/glm.hpp"

class Light {
  unsigned int depthMapFBO;
  unsigned int depthMap;
  const unsigned int SHADOW_WIDTH = 1024, SHADOW_HEIGHT = 1024;
public:
  Light(glm::vec3 color);
  void renderDepthMap(std::function<void()>);
  glm::vec3 color;
};

class LightPersister: public SQLPersisterImpl {
public:
  LightPersister(std::shared_ptr<EntityRegistry> registry)
    : SQLPersisterImpl("Light", registry){};
  void createTablesIfNeeded() override;
  void saveAll() override;
  void save(entt::entity) override;
  void loadAll() override;
  void load(entt::entity) override;
  void depersistIfGone(entt::entity) override;
};


