#include "components/Key.h"
#include <utility>

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
          "lockable_id INTEGER"
          "turn_movement_id INTEGER, "
          "unturn_movement_id INTEGER, "
          "state INTEGER, "
          "FOREIGN KEY (entity_id) REFERENCES Entity(id), "
          "FOREIGN KEY (lockable_id) REFERENCES Entity(id), "
          "FOREIGN KEY (turn_movement_id) REFERENCES RotateMovement(id), "
          "FOREIGN KEY (unturn_movement_id) REFERENCES RotateMovement(id) "
          ")");
}

void updateKey(SQLite::Database &db, int entityId,  TurnState state, entt::entity lockable) {
  SQLite::Statement updateKey(db, "UPDATE Key SET "
                                   "state = ?, lockable_id = ? "
                                   "WHERE entity_id = ?");

  updateKey.bind(1, (int)state);
  updateKey.bind(2, (int)lockable);
  updateKey.bind(3, entityId);

  updateKey.exec();
}

void insertKey(SQLite::Database &db, int entityId,
               int turnMovementId, int unturnMovementId,
               TurnState state, entt::entity lockableId) {
  std::stringstream insertKeyQuery;
  insertKeyQuery << "INSERT INTO Key (entity_id, turn_movement_id, "
                     "unturn_movement_id, state, lockable_id) "
                  << "VALUES (?, ?, ?, ?, ?)";
  SQLite::Statement insertKeyStmt(db, insertKeyQuery.str());

  insertKeyStmt.bind(1, entityId);
  insertKeyStmt.bind(2, turnMovementId);
  insertKeyStmt.bind(3, unturnMovementId);
  insertKeyStmt.bind(4, static_cast<int>(state));
  // what is static_cast again?
  insertKeyStmt.bind(5, static_cast<int>(lockableId));

  insertKeyStmt.exec();
}

void KeyPersister::saveAll() {
    auto view = registry->view<Persistable, Key>();
    SQLite::Database &db = registry->getDatabase();

    db.exec("BEGIN TRANSACTION");
    for (auto [entity, persist, key] : view.each()) {
      SQLite::Statement checkKey(db, "SELECT id, turn_movement_id, unturn_movement_id FROM Key WHERE entity_id = ?");
      checkKey.bind(1, persist.entityId);

      if (checkKey.executeStep()) {
        int turnMovementId = checkKey.getColumn(1).getInt();
        int unturnMovementId = checkKey.getColumn(2).getInt();

        updateMovement(db, turnMovementId, key.turnMovement);
        updateMovement(db, unturnMovementId, key.unturnMovement);

        updateKey(db, persist.entityId, key.state, key.lockable);
      } else {
        int openMovementId = insertMovement(db, key.turnMovement);
        int closeMovementId = insertMovement(db, key.unturnMovement);
        insertKey(db, persist.entityId, openMovementId, closeMovementId,
                  key.state, key.lockable);
      }
    }
    db.exec("COMMIT");
};
void KeyPersister::save(entt::entity){};

void KeyPersister::loadAll() {
    auto view = registry->view<Persistable>();
    SQLite::Database& db = registry->getDatabase();

    // Cache query data
    std::unordered_map<int, Key> keyDataCache;

    // Prepare the query
    SQLite::Statement query(db, "SELECT entity_id, turn_movement_id, unturn_movement_id, state, lockable_id FROM Key");

    // Fetch results from the database
    while (query.executeStep()) {
        int entityId = query.getColumn(0).getInt();
        int turnMovementId = query.getColumn(1).getInt();
        int unturnMovementId = query.getColumn(2).getInt();
        TurnState state = static_cast<TurnState>(query.getColumn(3).getInt());
        int lockableId = query.getColumn(3).getInt();

        auto turnMovement = getMovementData(db, turnMovementId);
        auto unturnMovement = getMovementData(db, unturnMovementId);
        auto pair = std::pair(entityId, Key{(entt::entity)lockableId, state, turnMovement, unturnMovement});
        keyDataCache.insert(pair);
    }

    for (auto [entity, persist] : view.each()) {
        auto it = keyDataCache.find(persist.entityId);
        if (it != keyDataCache.end()) {
          auto& [lockable, state, turnMovement, unturnMovement] = it->second;
          registry->emplace<Key>(entity, lockable, state, turnMovement, unturnMovement);
        }
    }
}
void KeyPersister::load(entt::entity){};
