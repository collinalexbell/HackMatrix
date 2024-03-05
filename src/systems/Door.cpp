#include "components/Door.h"
#include "persister.h"
#include "systems/Door.h"
#include <utility>

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
    auto movement = door.closeMovement;
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

void updateMovement(SQLite::Database &db, int movementId,
                    const RotateMovement &movement) {
  SQLite::Statement updateMovementStmt(
      db,
      "UPDATE RotateMovement SET "
      "axis_x = ?, axis_y = ?, axis_z = ?, degrees = ?, degrees_per_second = ?"
      "WHERE id = ?");

  updateMovementStmt.bind(1, movement.axis.x);
  updateMovementStmt.bind(2, movement.axis.y);
  updateMovementStmt.bind(3, movement.axis.z);
  updateMovementStmt.bind(4, movement.degrees);
  updateMovementStmt.bind(5, movement.degreesPerSecond);
  updateMovementStmt.bind(6, movementId);
  updateMovementStmt.exec();
}

int insertMovement(SQLite::Database &db,
                                           const RotateMovement &movement) {
  std::stringstream insertMovementQuery;
  insertMovementQuery << "INSERT INTO RotateMovement (axis_x, axis_y, axis_z, "
                         "degrees, degrees_per_second) "
                      << "VALUES (?, ?, ?, ?, ?)";
  SQLite::Statement insertMovementStmt(db, insertMovementQuery.str());

  insertMovementStmt.bind(1, movement.axis.x);
  insertMovementStmt.bind(2, movement.axis.y);
  insertMovementStmt.bind(3, movement.axis.z);
  insertMovementStmt.bind(4, movement.degrees);
  insertMovementStmt.bind(5, movement.degreesPerSecond);
  insertMovementStmt.exec();

  int movementId = db.getLastInsertRowid();
  return movementId;
}

void updateDoor(SQLite::Database &db, int entityId,  DoorState state) {
  SQLite::Statement updateDoor(db, "UPDATE Door SET "
                                   "state = ? "
                                   "WHERE entity_id = ?");

  updateDoor.bind(1, (int)state);
  updateDoor.bind(2, entityId);

  updateDoor.exec();
}

void insertDoor(SQLite::Database &db, int entityId,
                                        int openMovementId, int closeMovementId,
                                        DoorState state) {
  std::stringstream insertDoorQuery;
  insertDoorQuery << "INSERT INTO Door (entity_id, open_movement_id, "
                     "close_movement_id, state) "
                  << "VALUES (?, ?, ?, ?)";
  SQLite::Statement insertDoorStmt(db, insertDoorQuery.str());

  insertDoorStmt.bind(1, entityId);
  insertDoorStmt.bind(2, openMovementId);
  insertDoorStmt.bind(3, closeMovementId);
  insertDoorStmt.bind(4, static_cast<int>(state));

  insertDoorStmt.exec();
}

void systems::DoorPersister::saveAll() {
    auto view = registry->view<Persistable, Door>();
    SQLite::Database &db = registry->getDatabase();

    db.exec("BEGIN TRANSACTION");
    for (auto [entity, persist, door] : view.each()) {
      // Check if Door exists
      SQLite::Statement checkDoor(db, "SELECT id, open_movement_id, close_movement_id FROM Door WHERE entity_id = ?");
      checkDoor.bind(1, persist.entityId);

      if (checkDoor.executeStep()) {
        int openMovementId = checkDoor.getColumn(1).getInt();
        int closeMovementId = checkDoor.getColumn(2).getInt();

        updateMovement(db, openMovementId, door.openMovement);
        updateMovement(db, closeMovementId, door.closeMovement);

        updateDoor(db, persist.entityId, door.state);
      } else {
        int openMovementId = insertMovement(db, door.openMovement);
        int closeMovementId = insertMovement(db, door.closeMovement);
        insertDoor(db, persist.entityId, openMovementId, closeMovementId,
                   door.state);
      }
    }
    db.exec("COMMIT");
}

void systems::DoorPersister::save(entt::entity) {
}

RotateMovement getMovementData(SQLite::Database &db, int movementId) {
  // Prepare query
  SQLite::Statement query(
      db, "SELECT axis_x, axis_y, axis_z, degrees, degrees_per_second FROM "
          "RotateMovement WHERE id = ?");
  query.bind(1, movementId);

  if (query.executeStep()) {
    // Extract data and construct RotateMovement object
    double axisX = query.getColumn(0).getDouble();
    double axisY = query.getColumn(1).getDouble();
    double axisZ = query.getColumn(2).getDouble();
    double degrees = query.getColumn(3).getDouble();
    double degreesPerSecond = query.getColumn(4).getDouble();

    return RotateMovement(degrees, degreesPerSecond,
                          glm::vec3(axisX, axisY, axisZ));
  } else {
    // Handle the case where the movement is not found
    throw std::runtime_error("Movement not found in database");
  }
}

void systems::DoorPersister::loadAll() {
    auto view = registry->view<Persistable>();
    SQLite::Database& db = registry->getDatabase();

    // Cache query data
    std::unordered_map<int, Door> doorDataCache;

    // Prepare the query
    SQLite::Statement query(db, "SELECT entity_id, open_movement_id, close_movement_id, state FROM Door");

    // Fetch results from the database
    while (query.executeStep()) {
        int entityId = query.getColumn(0).getInt();
        int openMovementId = query.getColumn(1).getInt();
        int closeMovementId = query.getColumn(2).getInt();
        DoorState state = static_cast<DoorState>(query.getColumn(3).getInt());

        auto openMovement = getMovementData(db, openMovementId);
        auto closeMovement = getMovementData(db, closeMovementId);
        auto pair = std::pair(entityId, Door{openMovement, closeMovement, state});
        doorDataCache.insert(pair);
    }

    // Construct Door Components
    for (auto [entity, persist] : view.each()) {
        auto it = doorDataCache.find(persist.entityId);
        if (it != doorDataCache.end()) {
          auto& [openMovement, closeMovement, state] = it->second;
          registry->emplace<Door>(entity, openMovement, closeMovement, state);
        }
    }
}


void systems::DoorPersister::load(entt::entity) {
}

void systems::DoorPersister::depersistIfGone(entt::entity entity) {
  auto persistable = registry->get<Persistable>(entity);
  auto &db = registry->getDatabase();
  SQLite::Statement query(db, "SELECT open_movement_id, close_movement_id FROM Door where entity_id = ?");
  query.bind(1, persistable.entityId);
  query.exec();
  int64_t open_movement_id = query.getColumn(0).getInt64();
  int64_t close_movment_id = query.getColumn(1).getInt64();

  SQLite::Statement deleteQuery(db, "DELETE FROM RotateMovement WHERE id = ?");
  deleteQuery.bind(1, open_movement_id);
  deleteQuery.exec();
  deleteQuery.reset();
  deleteQuery.bind(1, close_movment_id);
  deleteQuery.exec();

  depersistIfGoneTyped<DoorPersister>(entity);
}
