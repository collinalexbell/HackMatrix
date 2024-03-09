
#include "systems/Intersections.h"

namespace systems {
  void emplaceBoundingSphere(std::shared_ptr<EntityRegistry> registry,
                                       entt::entity entity) {
    glm::vec3 center;
    float radius;
    registry->emplace<BoundingSphere>(entity, center, radius);
  }
}
