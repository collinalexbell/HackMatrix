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
  SQLite::Database &db = registry->getDatabase();

  db.exec("CREATE TABLE IF NOT EXISTS RotateMovement ( "
          "id INTEGER PRIMARY KEY AUTOINCREMENT, "
          "axis_x REAL, "
          "axis_y REAL, "
          "axis_z REAL, "
          "degrees REAL, "
          "degrees_per_second REAL "
          ")");

  db.exec("CREATE TABLE IF NOT EXISTS Door ( "
          "id INTEGER PRIMARY KEY, "
          "entity_id INTEGER, "
          "open_movement_id INTEGER, "
          "close_movement_id INTEGER, "
          "state INTEGER, "
          "FOREIGN KEY (entity_id) REFERENCES Entity(id), "
          "FOREIGN KEY (open_movement_id) REFERENCES RotateMovement(id), "
          "FOREIGN KEY (close_movement_id) REFERENCES RotateMovement(id) "
          ")");
}

void systems::DoorPersister::saveAll() {
}

void systems::DoorPersister::save(entt::entity) {
}

void systems::DoorPersister::loadAll() {
}

void systems::DoorPersister::load(entt::entity) {
}

void systems::DoorPersister::depersistIfGone(entt::entity) {}
