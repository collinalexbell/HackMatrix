#include "components/Bootable.h"

void BootablePersister::createTablesIfNeeded() {
  // even the pid should get saved (used for killOnExit = false)

  SQLite::Database &db = registry->getDatabase();

  std::stringstream create;

  create << "CREATE TABLE IF NOT EXISTS "
         << entityName << " ( "
         << "entity_id INTEGER PRIMARY KEY, "
         << "cmd TEXT,"
         << "args TEXT, "
         << "kill_on_exit INTEGER, "
         << "pid INTEGER, "
         << "FOREIGN KEY (entity_id) REFERENCES Entity(id), "
         <<")";
  db.exec(create.str());
}
void BootablePersister::saveAll() {
  // even the pid should get saved (used for killOnExit = false)
}
void BootablePersister::save(entt::entity){}
void BootablePersister::loadAll(){}
void BootablePersister::load(entt::entity){}
void BootablePersister::depersistIfGone(entt::entity){}
