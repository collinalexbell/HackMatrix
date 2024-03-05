#pragma once

#include "glm/geometric.hpp"
#include "glm/glm.hpp"
struct RotateMovement {
  RotateMovement(double degrees, double degreesPerSecond, glm::vec3 axis)
      : degrees(degrees), degreesPerSecond(degreesPerSecond) {
    this->axis = glm::normalize(axis);
  };
  glm::vec3 axis;
  double degrees;
  double degreesPerSecond;
};
