#include "components/Door.h"
#include "systems/Door.h"

void systems::openDoor(std::shared_ptr<EntityRegistry> registry, entt::entity entity) {
  auto &door = registry->get<Door>(entity);
  if(door.state == CLOSED) {
    auto movement = door.openMovement;
    movement.onFinish = [registry, entity]() -> void {
      auto &door = registry->get<Door>(entity);
      door.state = OPEN;
    };
    registry->emplace<RotateMovement>(entity, movement);
    door.state = OPENING;
  }
}

void systems::closeDoor(std::shared_ptr<EntityRegistry> registry, entt::entity entity) {
  auto &door = registry->get<Door>(entity);
  if(door.state == OPEN) {
    auto movement = door.openMovement;
    movement.onFinish = [registry, entity]() -> void {
      auto &door = registry->get<Door>(entity);
      door.state = CLOSED;
    };
    registry->emplace<RotateMovement>(entity, movement);
    door.state = CLOSING;
  }
}


void systems::DoorPersister::createTablesIfNeeded() {
}

void systems::DoorPersister::saveAll() {
}

void systems::DoorPersister::save(entt::entity) {
}

void systems::DoorPersister::loadAll() {
}

void systems::DoorPersister::load(entt::entity) {
}
