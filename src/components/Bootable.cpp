#include "components/Bootable.h"
#include <optional>

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
         << "FOREIGN KEY (entity_id) REFERENCES Entity(id)"
         <<")";
  db.exec(create.str());
}
void BootablePersister::saveAll() {
    auto view = registry->view<Persistable, Bootable>();
    SQLite::Database &db = registry->getDatabase();

    stringstream queryStream;
    queryStream << "INSERT INTO " << entityName << " "
                << "(entity_id, cmd, args, kill_on_exit, pid)"
                << "VALUES (?, ?, ?, ?, ?)";
    SQLite::Statement query(db, queryStream.str());

    db.exec("BEGIN TRANSACTION");
    for (auto [entity, persist, bootable] : view.each()) {
      query.bind(1, persist.entityId);
      query.bind(2, bootable.cmd);
      query.bind(3, bootable.args);
      query.bind(4, bootable.killOnExit ? 1 : 0);
      if(bootable.pid.has_value()) {
        query.bind(5, bootable.pid.value());
      } else {
        query.bind(5, nullptr);
      }
      query.exec();
      query.reset();
    }
    db.exec("COMMIT");
}

void BootablePersister::save(entt::entity){}
void BootablePersister::loadAll() {
  auto view = registry->view<Persistable>();
  SQLite::Database &db = registry->getDatabase();

  stringstream queryStream;
  queryStream << "SELECT entity_id, cmd, args, kill_on_exit, pid "
              << "FROM " << entityName;
  SQLite::Statement query(db, queryStream.str());

  while(query.executeStep()) {
    auto entityId = query.getColumn(0).getInt();
    auto cmd = query.getColumn(1).getText();
    auto args = query.getColumn(2).getText();
    bool killOnExit = query.getColumn(3).getInt() == 0 ? false : true;
    optional<int> pid;
    if(query.isColumnNull(4)) {
      pid = nullopt;
    } else {
      pid = query.getColumn(4).getInt();
    }
    auto entity = registry->locateEntity(entityId);

    if(entity.has_value()) {
      registry->emplace<Bootable>(entity.value(), cmd, args, killOnExit, pid);
    }
  }
}
void BootablePersister::load(entt::entity){}
void BootablePersister::depersistIfGone(entt::entity entity) {
  depersistIfGoneTyped<Bootable>(entity);
}
