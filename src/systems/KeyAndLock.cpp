#include "systems/KeyAndLock.h"
#include "components/Key.h"
#include "components/Lock.h"

void systems::turnKey(std::shared_ptr<EntityRegistry> registry, entt::entity entity) {
  auto &key = registry->get<Key>(entity);
  if (key.state == UNTURNED) {
    auto movement = key.turnMovement;
    movement.onFinish = [registry, entity]() -> void {
      auto &door = registry->get<Key>(entity);
      door.state = TURNED;
    };
    registry->emplace<RotateMovement>(entity, movement);
    key.state = TURNING;
  }
}
void systems::unturnKey(std::shared_ptr<EntityRegistry> registry, entt::entity entity) {
  auto &key = registry->get<Key>(entity);
  if (key.state == TURNED) {
    auto movement = key.unturnMovement;
    movement.onFinish = [registry, entity]() -> void {
      auto &door = registry->get<Key>(entity);
      door.state = UNTURNED;
    };
    registry->emplace<RotateMovement>(entity, movement);
    key.state = UNTURNING;
  }
}
