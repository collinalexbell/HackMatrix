#include "components/Key.h"

void KeyPersister::createTablesIfNeeded() {
  SQLite::Database &db = registry->getDatabase();

  /* This exists in Door.cpp. Should refactor into RotateMovement component
  db.exec("CREATE TABLE IF NOT EXISTS RotateMovement ( "
          "id INTEGER PRIMARY KEY AUTOINCREMENT, "
          "axis_x REAL, "
          "axis_y REAL, "
          "axis_z REAL, "
          "degrees REAL, "
          "degrees_per_second REAL "
          ")");
  */

  db.exec("CREATE TABLE IF NOT EXISTS  ( "
          "id INTEGER PRIMARY KEY, "
          "entity_id INTEGER, "
          "turn_movement_id INTEGER, "
          "unturn_movement_id INTEGER, "
          "state INTEGER, "
          "FOREIGN KEY (entity_id) REFERENCES Entity(id), "
          "FOREIGN KEY (turn_movement_id) REFERENCES RotateMovement(id), "
          "FOREIGN KEY (unturn_movement_id) REFERENCES RotateMovement(id) "
          ")");
}

void KeyPersister::saveAll(){};
void KeyPersister::save(entt::entity){};
void KeyPersister::loadAll(){}
void KeyPersister::load(entt::entity){};
