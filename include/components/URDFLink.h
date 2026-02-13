#pragma once

#include <entt.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include "SQLPersisterImpl.h"
#include "persister.h"

struct URDFLink
{
  enum class JointType
  {
    Fixed,
    Revolute,
    Prismatic
  };

  entt::entity parent = entt::null;

  // Joint origin (relative to parent link frame).
  glm::vec3 originPos = glm::vec3(0.0f);
  glm::vec3 originRpy = glm::vec3(0.0f); // radians

  // Joint axis (unit vector in joint frame).
  glm::vec3 axis = glm::vec3(0.0f, 0.0f, 1.0f);

  JointType jointType = JointType::Fixed;
  float jointValue = 0.0f; // radians for revolute, meters for prismatic

  // World position and rotation in URDF space (for root links)
  glm::vec3 urdfWorldPos = glm::vec3(0.0f);
  glm::vec3 urdfWorldRot = glm::vec3(0.0f); // degrees
  
  // World rotation as quaternion (computed, avoids Euler roundtrip errors)
  glm::quat urdfWorldRotQuat = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

  // Visual origin rotation (RPY in radians, like originRpy)
  // This is the rotation from the visual origin tag in the URDF
  glm::vec3 visualOriginRpy = glm::vec3(0.0f); // radians

  bool dirty = true;
};

class URDFLinkPersister : public SQLPersisterImpl
{
public:
  URDFLinkPersister(std::shared_ptr<EntityRegistry> registry)
    : SQLPersisterImpl("URDFLink", registry){};
  void createTablesIfNeeded() override;
  void saveAll() override;
  void save(entt::entity) override;
  void loadAll() override;
  void load(entt::entity) override;
  void depersistIfGone(entt::entity) override;
};
