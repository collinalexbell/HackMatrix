
#include "systems/Intersections.h"
#include "model.h"


namespace systems {
  void emplaceBoundingSphere(std::shared_ptr<EntityRegistry> registry,
                                       entt::entity entity) {
    auto [model, positionable] = registry->get<Model, Positionable>(entity);

    auto boundingSphere = model.getBoundingSphere();
    registry->emplace_or_replace<BoundingSphere>(entity,
                                                 boundingSphere.center,
                                                 boundingSphere.radius);
  }
}
