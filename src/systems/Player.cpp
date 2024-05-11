#include "systems/Player.h"
#include <map>
#include <utility>
#include "model.h"
#include "systems/Move.h"

namespace systems {
  std::map<uint32_t, entt::entity> registeredPlayers;
  void registerPlayer(std::shared_ptr<EntityRegistry> registry,
      uint32_t connectionId)  {
    auto entity = registry->create();
    registry->emplace<Positionable>(entity, glm::vec3(0), glm::vec3(0), glm::vec3(0), 1.0);
    registry->emplace<Model>(entity, "vox/hacker.obj");
    registeredPlayers.insert(std::make_pair(connectionId, entity));
  }
  void movePlayer(std::shared_ptr<EntityRegistry> registry, uint32_t connectionId,
      glm::vec3 position, glm::vec3 front, float time) {
    if(registeredPlayers.contains(connectionId)) {
      auto entity = registeredPlayers[connectionId];
      auto positionable = registry->get<Positionable>(entity);
      auto delta = position - positionable.pos;
      auto length = glm::length(delta);
      auto unitsPerSecond = length / time;
      systems::translate(registry, entity, delta, unitsPerSecond);
    }
  }
}
