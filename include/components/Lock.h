#pragma once

#include "glm/glm.hpp"
enum LockState {
  UNLOCKED, LOCKED
};
struct Lock {
  glm::vec3 position;
  glm::vec3 tolerance;
  LockState state;
};
