#pragma once

#include "glm/geometric.hpp"
#include "glm/glm.hpp"
#include <functional>
#include <optional>

#include <SQLiteCpp/SQLiteCpp.h>

struct RotateMovement {
  RotateMovement(double degrees, double degreesPerSecond, glm::vec3 axis)
      : degrees(degrees), degreesPerSecond(degreesPerSecond) {
    this->axis = glm::normalize(axis);
  };
  glm::vec3 axis;
  double degrees;
  double degreesPerSecond;
  std::optional<std::function<void()>> onFinish;
};

RotateMovement getMovementData(SQLite::Database &db, int movementId);
int insertMovement(SQLite::Database &db, const RotateMovement &movement);
void updateMovement(SQLite::Database &db, int movementId, const RotateMovement &movement);
