#pragma once

#include "SQLPersisterImpl.h"
#include <functional>
#include "glm/glm.hpp"

class Light {
  static unsigned int nextTextureUnit;
  unsigned int depthMapFBO;
  const unsigned int SHADOW_WIDTH = 1024, SHADOW_HEIGHT = 1024;

  void lightspaceTransform(glm::vec3);
public:
  unsigned int textureUnit;
  unsigned int depthCubemap;
  Light(glm::vec3 color);
  void renderDepthMap(glm::vec3 lightPos, std::function<void()>);
  glm::vec3 color;
  std::vector<glm::mat4> shadowTransforms;
  float nearPlane;
  float farPlane;
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


