#include "systems/Light.h"
#include "components/Light.h"
#include "model.h"
#include "renderer.h"

void
systems::updateLighting(std::shared_ptr<EntityRegistry> registry,
                        Renderer* renderer)
{
  auto view = registry->view<Light, Positionable>();
  for (auto [entity, light, positionable] : view.each()) {
    std::function<void()> render =
      std::bind(&Renderer::render, renderer, LIGHT, entity);
    light.renderDepthMap(positionable.pos, render);
  }
}
