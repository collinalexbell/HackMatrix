#include "systems/Player.h"
#include <map>
#include <utility>
#include "glm/gtc/constants.hpp"
#include "model.h"
#include "systems/Move.h"
#include "glm/gtx/transform.hpp"
#include "glm/ext/quaternion_trigonometric.hpp"
#include "glm/gtx/rotate_vector.hpp"
#include <glm/gtx/euler_angles.hpp>
#include "glm/glm.hpp"
#include <glm/gtc/quaternion.hpp>

namespace systems {
std::map<uint32_t, entt::entity> registeredPlayers;
void
registerPlayer(std::shared_ptr<EntityRegistry> registry, uint32_t connectionId)
{
  auto entity = registry->create();
  registry->emplace<Positionable>(
    entity, glm::vec3(0), glm::vec3(0, 0.85, 0), glm::vec3(0), 0.3);
  registry->emplace<Model>(entity, "vox/hacker.obj");
  registeredPlayers.insert(std::make_pair(connectionId, entity));
}
void
movePlayer(std::shared_ptr<EntityRegistry> registry,
           uint32_t connectionId,
           glm::vec3 position,
           glm::vec3 front,
           float time)
{
  if (registeredPlayers.contains(connectionId)) {
    auto entity = registeredPlayers[connectionId];
    auto& positionable = registry->get<Positionable>(entity);
    auto delta = position - positionable.pos;
    auto length = glm::length(delta);
    auto unitsPerSecond = length / time;
    // systems::translate(registry, entity, delta, unitsPerSecond);
    //

    float frontX = front.x;
    float frontZ = front.z;

    // Calculate the y-rotation angle in radians using atan2
    float yRotationAngle = atan2(frontX, frontZ);

    positionable.pos = position;
    positionable.rotate = glm::vec3(0.0f, glm::degrees(yRotationAngle), 0.0f);
    positionable.update();
  }
}
}
