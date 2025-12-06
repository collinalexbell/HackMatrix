#include "components/Light.h"
#include "glad/glad.h"
#include "glm/gtc/matrix_transform.hpp"
#include <sstream>
#include <iostream>
#include "stb/stb_image_write.h"
#include "tracy/TracyOpenGL.hpp"

unsigned int Light::nextTextureUnit = 20;

Light::Light(glm::vec3 color): color(color) {
  textureUnit = nextTextureUnit++;
  std::cout << "textureUnit: " << textureUnit << std::endl;
  farPlane = 50.0f;
  nearPlane = 0.02f;

  glGenFramebuffers(1, &depthMapFBO);
  glGenTextures(1, &depthCubemap);
  glActiveTexture(GL_TEXTURE0 + textureUnit);
  glBindTexture(GL_TEXTURE_CUBE_MAP, depthCubemap);

  // Allocate storage for each face of the depth cubemap
  for (unsigned int i = 0; i < 6; ++i) {
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0,
        GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT,
        0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL); 
  }

  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

  GLint prevFbo = 0;
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFbo);
  glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);

  glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthCubemap, 0);

  glDrawBuffer(GL_NONE);
  glReadBuffer(GL_NONE);

  glBindFramebuffer(GL_FRAMEBUFFER, prevFbo);
}

void Light::renderDepthMap(glm::vec3 lightPos, std::function<void()> renderScene) {
  int viewport[4];
  glGetIntegerv(GL_VIEWPORT, viewport);
  int SCR_WIDTH = viewport[2];
  int SCR_HEIGHT = viewport[3];
  lightspaceTransform(lightPos);
  glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
  GLint prevFbo = 0;
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFbo);
  glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
  glClear(GL_DEPTH_BUFFER_BIT);
  TracyGpuZone("renderDepthMap");
  renderScene();
  glBindFramebuffer(GL_FRAMEBUFFER, prevFbo);
  glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
}

void Light::lightspaceTransform(glm::vec3 lightPos) {
  std::cout << "lightspace transform: " << lightPos.x << "," << lightPos.y << ", " << lightPos.z << std::endl;
  float aspect = (float)SHADOW_WIDTH/(float)SHADOW_HEIGHT;
  glm::mat4 shadowProj = glm::perspective(glm::radians(90.0f),
      aspect, nearPlane, farPlane);
  shadowTransforms.clear();
  shadowTransforms.push_back(shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)));
  shadowTransforms.push_back(shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)));
  shadowTransforms.push_back(shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)));
  shadowTransforms.push_back(shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)));
  shadowTransforms.push_back(shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f)));
  shadowTransforms.push_back(shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f)));
  
}

void LightPersister::createTablesIfNeeded() {
  SQLite::Database &db = registry->getDatabase();
  std::stringstream createTableStream;
  createTableStream << "CREATE TABLE IF NOT EXISTS " << entityName << " ("
                    << "entity_id INTEGER PRIMARY KEY, "
                    << "color_r REAL, color_g REAL, color_b REAL, "
                    << "FOREIGN KEY(entity_id) REFERENCES Entity(id)) ";
  db.exec(createTableStream.str());
}

void LightPersister::loadAll() {
  auto view = registry->view<Persistable>();
  SQLite::Database &db = registry->getDatabase();

  // Cache query data
  std::unordered_map<int, glm::vec3> lightDataCache;
  std::stringstream queryStream;
  queryStream << "SELECT entity_id, color_r, color_g, color_b FROM " << entityName;
  SQLite::Statement query(db, queryStream.str());

  while (query.executeStep()) {
    int entityId = query.getColumn(0).getInt();
    float r = query.getColumn(1).getDouble();
    float g = query.getColumn(2).getDouble();
    float b = query.getColumn(3).getDouble();

    lightDataCache[entityId] = glm::vec3(r, g, b);
  }

  // Iterate and emplace
  view.each([&lightDataCache, this](auto entity, auto &persistable) {
    auto it = lightDataCache.find(persistable.entityId);
    if (it != lightDataCache.end()) {
      auto &color = it->second;
      registry->emplace<Light>(entity, color);
    }
  });
}

void LightPersister::saveAll() {
  auto view = registry->view<Persistable, Light>();
  SQLite::Database &db = registry->getDatabase();
  std::stringstream queryStream;
  queryStream << "INSERT OR REPLACE INTO " << entityName
              << " (entity_id, color_r, color_g, color_b) VALUES (?, ?, ?, ?)";
  SQLite::Statement query(db, queryStream.str());

  db.exec("BEGIN TRANSACTION");

  for (auto [entity, persist, light] : view.each()) {
    query.bind(1, persist.entityId);
    query.bind(2, light.color.x);
    query.bind(3, light.color.y);
    query.bind(4, light.color.z);
    query.exec();
    query.reset();
  }

  db.exec("COMMIT");
}

void LightPersister::save(entt::entity entity) {
  auto &light = registry->get<Light>(entity);
  auto &persistable = registry->get<Persistable>(entity);
  SQLite::Database &db = registry->getDatabase();

  std::stringstream queryStream;
  queryStream << "INSERT OR REPLACE INTO " << entityName
             << " (entity_id, color_r, color_g, color_b) VALUES (?, ?, ?, ?)";
  SQLite::Statement query(db, queryStream.str());
  query.bind(1, persistable.entityId);
  query.bind(2, light.color.x);
  query.bind(3, light.color.y);
  query.bind(4, light.color.z);
  query.exec();
}

void LightPersister::load(entt::entity entity) {
  auto &persistable = registry->get<Persistable>(entity);
  SQLite::Database &db = registry->getDatabase();

  std::stringstream queryStream;
  queryStream << "INSERT OR REPLACE INTO " << entityName
              << " (entity_id, color_r, color_g, color_b) VALUES (?, ?, ?, ?)";
  SQLite::Statement query(db, queryStream.str());
  query.bind(1, persistable.entityId);

  if (query.executeStep()) {
    float r = query.getColumn(0).getDouble();
    float g = query.getColumn(1).getDouble();
    float b = query.getColumn(2).getDouble();

    registry->emplace<Light>(entity, glm::vec3(r, g, b));
  }
}

void LightPersister::depersistIfGone(entt::entity entity) {
  depersistIfGoneTyped<Light>(entity);
}
