#include "systems/KeyAndLock.h"
#include "components/Key.h"
#include "components/Lock.h"
#include "model.h"
#include "systems/Door.h"

void systems::turnKey(std::shared_ptr<EntityRegistry> registry, entt::entity entity) {
  auto &key = registry->get<Key>(entity);
  if (key.state == UNTURNED) {
    auto movement = key.turnMovement;
    movement.onFinish = [registry, entity]() -> void {
      auto [key, keyPos] = registry->get<Key, Positionable>(entity);
      key.state = TURNED;
      auto lockView = registry->view<Persistable, Lock, Positionable>();
      for(auto [entity, persistable, lock, positionable]: lockView.each()) {
        if(persistable.entityId == (int)key.lockable) {
          auto distances = (lock.position + positionable.pos) - keyPos.pos;
          if (lock.state == LOCKED &&
              distances.x <= lock.tolerance.x &&
              distances.y <= lock.tolerance.y &&
              distances.z <= lock.tolerance.z) {
            lock.state = UNLOCKED;
            systems::openDoor(registry, entity);
          }
        }
      }
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
