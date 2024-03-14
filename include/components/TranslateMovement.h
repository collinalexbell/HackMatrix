#pragma once

#include "glm/geometric.hpp"
#include "glm/glm.hpp"
#include <functional>
#include <optional>

#include <SQLiteCpp/SQLiteCpp.h>

struct TranslateMovement {
TranslateMovement(glm::vec3 delta, double unitsPerSecond)
      : delta(delta), unitsPerSecond(unitsPerSecond) {};
  glm::vec3 delta;
  double unitsPerSecond;
  std::optional<std::function<void()>> onFinish;
};
