#pragma once

#include "entity.h"
#include "glm/glm.hpp"
#include <memory>

namespace systems {
  void translate(std::shared_ptr<EntityRegistry> registry,
                 entt::entity entity,
                 glm::vec3 delta,
                 double unitsPerSecond) ;
}
