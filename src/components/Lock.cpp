#include "components/Lock.h"

void LockPersister::createTablesIfNeeded() {
  auto &db = registry->getDatabase();
  std::stringstream create;
  create << "CREATE TABLE IF NOT EXISTS " << entityName << " ( "
         << "entity_id INTEGER PRIMARY KEY, "
         << "position_x REAL, position_y REAL, position_z REAL, "
         << "tolerance_x REAL, tolerance_y REAL, tolerance_z REAL, "
         << "state INTEGER, "
         << "FOREIGN KEY (entity_id) REFERENCES Entity(id)"
         << ")";

  db.exec(create.str());
}
void LockPersister::saveAll() {
   auto view = registry->view<Persistable, Lock>();

  SQLite::Database &db = registry->getDatabase(); // Get database reference

  std::stringstream queryStream;
  queryStream << "INSERT OR REPLACE INTO " << entityName
              << " (entity_id, position_x, position_y, position_z, "
              << "tolerance_x, tolerance_y, tolerance_z, "
              << "state)"
              << " VALUES (?, ?, ?, ?, ?, ?, ?, ?)";
    SQLite::Statement query(db, queryStream.str());
  // Use a transaction for efficiency
  db.exec("BEGIN TRANSACTION");

  for (auto [entity, persist, lock] : view.each()) {
    query.bind(1, persist.entityId);
    query.bind(2, lock.position.x);
    query.bind(3, lock.position.y);
    query.bind(4, lock.position.z);
    query.bind(5, lock.tolerance.x);
    query.bind(6, lock.tolerance.y);
    query.bind(7, lock.tolerance.z);
    query.bind(8, lock.state);
    query.exec();
    query.reset();
  }
  db.exec("COMMIT");
};
void LockPersister::save(entt::entity){};
void LockPersister::loadAll() {
    auto view = registry->view<Persistable>();
    SQLite::Database& db = registry->getDatabase();

    // Cache query data
    std::unordered_map<int, Lock> positionDataCache;

    std::stringstream queryStream;
    queryStream << "SELECT entity_id, position_x, position_y, position_z, "
                << "tolerance_x, tolerance_y, tolerance_z, "
                << "state FROM "
                << entityName;
    SQLite::Statement query(db, queryStream.str());
    while (query.executeStep()) {
      int dbId = query.getColumn(0).getInt();
      float x = query.getColumn(1).getDouble();
      float y = query.getColumn(2).getDouble();
      float z = query.getColumn(3).getDouble();
      float toleranceX = query.getColumn(4).getDouble();
      float toleranceY = query.getColumn(5).getDouble();
      float toleranceZ = query.getColumn(6).getDouble();
      float state = query.getColumn(7).getInt();

      Lock l = {glm::vec3(x,y,z),
        glm::vec3(toleranceX, toleranceY, toleranceZ),
        LockState(state)};
      positionDataCache[dbId] = l;
    }

    // Iterate and emplace
    for(auto [entity, persistable]: view.each()) {
        auto it = positionDataCache.find(persistable.entityId);
        if (it != positionDataCache.end()) {
          auto& [position, tolerance, state] = it->second;
          registry->emplace<Lock>(entity, position, tolerance, state);
        }
    }
}
void LockPersister::load(entt::entity){};
void LockPersister::depersistIfGone(entt::entity entity) {
  depersistIfGoneTyped<Lock>(entity);
};
