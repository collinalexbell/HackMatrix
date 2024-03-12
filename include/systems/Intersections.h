#pragma once
#include "components/BoundingSphere.h"
#include "entity.h"
#include <memory>

namespace systems {
  void emplaceBoundingSphere(std::shared_ptr<EntityRegistry>, entt::entity);
  bool intersect(const BoundingSphere &sphere,
                 glm::vec3 position,
                 glm::vec3 direction,
                 float maxDistance);
}
