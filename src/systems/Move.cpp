#include "systems/Move.h"
#include "components/TranslateMovement.h"


void systems::translate(std::shared_ptr<EntityRegistry> registry, entt::entity entity, glm::vec3 delta, double unitsPerSecond) {
  registry->emplace<TranslateMovement>(entity, delta, unitsPerSecond);
}
