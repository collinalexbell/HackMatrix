#include "systems/Update.h"
#include "components/BoundingSphere.h"
#include "model.h"
#include "systems/Intersections.h"

void systems::update(std::shared_ptr<EntityRegistry> registry, entt::entity entity) {
  auto &positionable = registry->get<Positionable>(entity);
  positionable.update();

  auto hasBoundingSphere = registry->all_of<BoundingSphere>(entity);
  if(hasBoundingSphere) {
    emplaceBoundingSphere(registry, entity);
  }
}
