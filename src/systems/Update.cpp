#include "systems/Update.h"
#include "systems/ApplyRotation.h"
#include "systems/ApplyTranslation.h"
#include "components/BoundingSphere.h"
#include "model.h"
#include "systems/Intersections.h"
#include "systems/Light.h"

void
systems::updateAll(std::shared_ptr<EntityRegistry> registry, Renderer* renderer)
{
  bool updatedSomething = false;
  auto view = registry->view<Positionable>();
  for (auto [entity, positionable] : view.each()) {
    if (positionable.damaged) {
      systems::update(registry, entity);
      updatedSomething = true;
    }
  }
  if (updatedSomething) {
    systems::updateLighting(registry, renderer);
  }

  systems::applyRotation(registry);
  systems::applyTranslations(registry);
}

void
systems::update(std::shared_ptr<EntityRegistry> registry, entt::entity entity)
{
  auto& positionable = registry->get<Positionable>(entity);
  positionable.update();

  auto hasBoundingSphere = registry->all_of<BoundingSphere>(entity);
  if (hasBoundingSphere) {
    emplaceBoundingSphere(registry, entity);
  }
}
