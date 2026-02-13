#include "systems/URDFTransform.h"
#include "components/URDFLink.h"
#include "model.h"
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/transform.hpp>
#include <unordered_set>
#include <unordered_map>

// Convert RPY (roll-pitch-yaw in radians) to quaternion
// URDF RPY order: roll (X) -> pitch (Y) -> yaw (Z)
// This means: first rotate around X by roll, then around Y by pitch, then around Z by yaw
// In quaternion multiplication (right-to-left application):
// To apply roll first, then pitch, then yaw: q_yaw * q_pitch * q_roll
static glm::quat
rpyToQuat(const glm::vec3& rpy)
{
  glm::quat q_roll = glm::angleAxis(rpy.x, glm::vec3(1.0f, 0.0f, 0.0f));  // roll around X
  glm::quat q_pitch = glm::angleAxis(rpy.y, glm::vec3(0.0f, 1.0f, 0.0f)); // pitch around Y
  glm::quat q_yaw = glm::angleAxis(rpy.z, glm::vec3(0.0f, 0.0f, 1.0f));   // yaw around Z
  
  // Standard order: roll first, then pitch, then yaw
  // Quaternion multiplication applies right-to-left, so: q_yaw * q_pitch * q_roll
  return q_yaw * q_pitch * q_roll;
}

void
systems::applyURDFTransforms(std::shared_ptr<EntityRegistry> registry)
{
  auto view = registry->view<URDFLink, Positionable>();
  if (view.begin() == view.end()) {
    return;
  }

  // Early exit: check if any links are dirty before doing expensive work
  bool anyDirty = false;
  for (auto [entity, link, positionable] : view.each()) {
    if (link.dirty) {
      anyDirty = true;
      break;
    }
  }
  if (!anyDirty) {
    return;
  }

  const size_t maxPasses = view.size_hint() + 1;
  std::unordered_set<entt::entity> updated;
  // Map to store URDF-space world positions and quaternions (following rviz pattern)
  std::unordered_map<entt::entity, glm::vec3> urdfWorldPositions;
  std::unordered_map<entt::entity, glm::quat> urdfWorldRotations;

  for (size_t pass = 0; pass < maxPasses; ++pass) {
    bool didWork = false;

    for (auto [entity, link, positionable] : view.each()) {
      // Skip entirely if not dirty (no work needed)
      if (!link.dirty) {
        continue;
      }

      if (link.parent == entt::null) {
        // Root link: use stored URDF position/rotation directly (no coordinate conversion)
        urdfWorldPositions[entity] = link.urdfWorldPos;
        urdfWorldRotations[entity] = rpyToQuat(glm::radians(link.urdfWorldRot));
        link.urdfWorldRotQuat = urdfWorldRotations[entity];
        
        // Use URDF values directly - no conversion (following rviz pattern)
        positionable.pos = urdfWorldPositions[entity];
        positionable.rotate = glm::degrees(glm::eulerAngles(urdfWorldRotations[entity]));
        // Preserve scale and origin - don't overwrite them
        positionable.damage();
        
        updated.insert(entity);
        link.dirty = false;
        didWork = true;
        continue;
      }

      if (!registry->valid(link.parent) ||
          !registry->all_of<Positionable>(link.parent)) {
        continue;
      }

      if (registry->all_of<URDFLink>(link.parent) &&
          !updated.contains(link.parent)) {
        continue;
      }

      // Get parent's URDF-space position and rotation
      glm::vec3 parentUrdfPos;
      glm::quat parentUrdfRot;
      if (registry->all_of<URDFLink>(link.parent)) {
        // Parent is a URDF link - use its URDF-space transform
        auto posIt = urdfWorldPositions.find(link.parent);
        auto rotIt = urdfWorldRotations.find(link.parent);
        if (posIt == urdfWorldPositions.end() || rotIt == urdfWorldRotations.end()) {
          continue; // Parent not processed yet
        }
        parentUrdfPos = posIt->second;
        parentUrdfRot = rotIt->second;
      } else {
        // Parent is not a URDF link - use identity transform in URDF space
        parentUrdfPos = glm::vec3(0.0f);
        parentUrdfRot = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
      }

      // Compose in URDF space using quaternion/vector operations (following rviz pattern)
      // Joint origin: position and rotation (RPY converted to quaternion)
      glm::quat jointOriginRot = rpyToQuat(link.originRpy);
      
      // Apply joint value rotation if needed (for revolute joints)
      // The axis is specified in the joint frame, so we rotate around it in that frame
      if (link.jointType == URDFLink::JointType::Revolute) {
        glm::vec3 axis = link.axis;
        if (glm::length(axis) > 0.001f) {
          axis = glm::normalize(axis);
        }
        // Rotate around axis in joint frame: joint_origin_rot * rotation_around_axis
        glm::quat jointRot = glm::angleAxis(link.jointValue, axis);
        jointOriginRot = jointOriginRot * jointRot;
      }
      
      // URDF joint origin: "translate by xyz, then rotate by rpy"
      // This means: T(xyz) * R(rpy) in the parent frame
      // When composing: child_world = parent_world * T(xyz) * R(rpy)
      // In quaternion/vector form:
      //   child_pos = parent_pos + parent_rot * xyz
      //   child_rot = parent_rot * rpy_quat
      // The xyz translation is in parent frame, so we rotate it by parent_rot
      glm::vec3 urdfWorldPos = parentUrdfPos + parentUrdfRot * link.originPos;
      glm::quat urdfWorldRot = parentUrdfRot * jointOriginRot;
      
      // Handle prismatic joints (translation along axis in joint frame)
      if (link.jointType == URDFLink::JointType::Prismatic) {
        glm::vec3 axis = link.axis;
        if (glm::length(axis) > 0.001f) {
          axis = glm::normalize(axis);
        }
        // Axis is in joint frame, transform to world frame: parent_rot * joint_origin_rot * axis
        glm::vec3 axisInWorld = urdfWorldRot * axis;
        urdfWorldPos += axisInWorld * link.jointValue;
      }
      
      urdfWorldPositions[entity] = urdfWorldPos;
      urdfWorldRotations[entity] = urdfWorldRot;

      // Store quaternion directly in URDFLink to avoid Euler roundtrip errors
      link.urdfWorldRotQuat = urdfWorldRot;
      
      // Use URDF values directly - no conversion (following rviz pattern)
      positionable.pos = urdfWorldPos;
      positionable.rotate = glm::degrees(glm::eulerAngles(urdfWorldRot));
      // Preserve scale and origin - don't overwrite them
      positionable.damage();

      updated.insert(entity);
      link.dirty = false;
      didWork = true;
    }

    if (!didWork) {
      break;
    }
  }
}
