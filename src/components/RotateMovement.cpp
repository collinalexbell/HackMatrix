#include "components/RotateMovement.h"

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

int insertMovement(SQLite::Database &db, const RotateMovement &movement) {
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
