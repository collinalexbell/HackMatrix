#include "systems/Derivative.h"
#include "components/Scriptable.h"
#include "entity.h"
#include "model.h"
#include "systems/Intersections.h"

void systems::createDerivativeComponents(std::shared_ptr<EntityRegistry> registry) {
  auto boundable = registry->view<Positionable, Scriptable>();
  for(auto [entity, _positionable, _scriptable]: boundable.each()) {
    systems::emplaceBoundingSphere(registry, entity);
  }
}
