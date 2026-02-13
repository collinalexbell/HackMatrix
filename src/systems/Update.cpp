#include "systems/Update.h"
#include "components/BoundingSphere.h"
#include "components/URDFLink.h"
#include "model.h"
#include "systems/Intersections.h"
#include "systems/Light.h"
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/transform.hpp>

// Convert RPY (roll-pitch-yaw in radians) to quaternion (same as URDFTransform.cpp)
static glm::quat
rpyToQuat(const glm::vec3& rpy)
{
  glm::quat q_roll = glm::angleAxis(rpy.x, glm::vec3(1.0f, 0.0f, 0.0f));  // roll around X
  glm::quat q_pitch = glm::angleAxis(rpy.y, glm::vec3(0.0f, 1.0f, 0.0f)); // pitch around Y
  glm::quat q_yaw = glm::angleAxis(rpy.z, glm::vec3(0.0f, 0.0f, 1.0f));   // yaw around Z
  return q_yaw * q_pitch * q_roll;
}

// Convert URDF position to graphics position
// URDF: X=backwards, Y=right, Z=up
// Graphics: X=right, Y=up, Z=out of screen (positive)
// Graphics X = URDF Y, Graphics Y = URDF Z, Graphics Z = -URDF X
static glm::vec3
urdfPosToGraphics(const glm::vec3& urdfPos)
{
  return glm::vec3(urdfPos.y, urdfPos.z, -urdfPos.x);
}

// Convert URDF quaternion to graphics quaternion
// The coordinate system rotation matrix from URDF to Graphics is:
// [0 1 0]   Graphics X = URDF Y
// [0 0 1]   Graphics Y = URDF Z
// [-1 0 0]  Graphics Z = -URDF X
// We convert by: q_graphics = q_coord_transform * q_urdf * q_coord_transform^-1
static glm::quat
urdfRotToGraphics(const glm::quat& urdfRot)
{
  // Coordinate transform rotation matrix
  glm::mat3 coordRot(
    0.0f, 1.0f, 0.0f,  // Graphics X = URDF Y
    0.0f, 0.0f, 1.0f,  // Graphics Y = URDF Z
    -1.0f, 0.0f, 0.0f  // Graphics Z = -URDF X
  );
  glm::mat3 urdfRotMat = glm::mat3_cast(urdfRot);
  glm::mat3 graphicsRotMat = coordRot * urdfRotMat * glm::transpose(coordRot);
  return glm::quat_cast(graphicsRotMat);
}

void
systems::updateAll(std::shared_ptr<EntityRegistry> registry, Renderer* renderer)
{
  bool updatedSomething = false;
  auto view = registry->view<Positionable>();
  for (auto [entity, positionable] : view.each()) {
    if (positionable.damaged) {
      systems::update(registry, entity);
      updatedSomething = true;
    }
  }
  if (updatedSomething) {
    systems::updateLighting(registry, renderer);
  }
}

void
systems::update(std::shared_ptr<EntityRegistry> registry, entt::entity entity)
{
  auto& positionable = registry->get<Positionable>(entity);
  
  // Apply visual origin rotation if this is a URDF link (like rviz does)
  // In rviz: visual_node has link's world transform, offset_node (child) has visual origin
  // Final transform: link_world * visual_origin = T(link_pos) * R(link_rot) * T(visual_origin_pos) * R(visual_origin_rot)
  if (registry->all_of<URDFLink>(entity)) {
    auto& link = registry->get<URDFLink>(entity);
    // Always apply visual origin rotation (even if zero, it will be identity)
    // Use quaternion directly to avoid Euler roundtrip errors
    // Convert from URDF space to graphics space for the model matrix (shader expects graphics coords)
    // Model matrix: T(link_pos_graphics) * R(link_rot_graphics) * T(visual_origin_pos_graphics) * R(visual_origin_rot_graphics) * S(scale)
    glm::vec3 graphicsPos = urdfPosToGraphics(positionable.pos);
    glm::quat graphicsRot = urdfRotToGraphics(link.urdfWorldRotQuat);
    glm::vec3 graphicsOrigin = urdfPosToGraphics(positionable.origin);
    glm::quat graphicsVisualOriginRot = urdfRotToGraphics(rpyToQuat(link.visualOriginRpy));
    
    positionable.modelMatrix = glm::mat4(1.0f);
    positionable.modelMatrix = glm::translate(positionable.modelMatrix, graphicsPos);
    positionable.modelMatrix = positionable.modelMatrix * glm::mat4_cast(graphicsRot);
    positionable.modelMatrix = glm::translate(positionable.modelMatrix, graphicsOrigin);
    positionable.modelMatrix = positionable.modelMatrix * glm::mat4_cast(graphicsVisualOriginRot);
    positionable.modelMatrix = glm::scale(positionable.modelMatrix, glm::vec3(positionable.scale, positionable.scale, positionable.scale));
    
    glm::mat4 inverseModelMatrix = glm::inverse(positionable.modelMatrix);
    glm::mat4 transposedInverse = glm::transpose(inverseModelMatrix);
    positionable.normalMatrix = glm::mat3(transposedInverse);
    positionable.damaged = false;
  } else {
    positionable.update();
  }

  auto hasBoundingSphere = registry->all_of<BoundingSphere>(entity);
  if (hasBoundingSphere) {
    emplaceBoundingSphere(registry, entity);
  }
}
