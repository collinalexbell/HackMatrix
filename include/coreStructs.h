#pragma once
#include <glm/glm.hpp>

struct Position {
  int x;
  int y;
  int z;
  bool valid;
  glm::vec3 normal;
};
