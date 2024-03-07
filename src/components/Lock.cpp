#include "components/Lock.h"

void LockPersister::createTablesIfNeeded() {
  auto &db = registry->getDatabase();
  std::stringstream create;
  create << "CREATE TABLE IF NOT EXISTS " << entityName << " ( "
         << "id INTEGER PRIMARY KEY, "
         << "entity_id INTEGER, "
         << "position_x REAL, position_y REAL, position_z REAL, "
         << "tolerance_x REAL, tolerance_y REAL, tolerance_z REAL, "
         << "state INTEGER, "
         << "FOREIGN KEY (entity_id) REFERENCES Entity(id)"
         << ")";

  db.exec(create.str());
}
void LockPersister::saveAll(){};
void LockPersister::save(entt::entity){};
void LockPersister::loadAll() {}
void LockPersister::load(entt::entity){};
void LockPersister::depersistIfGone(entt::entity){};
