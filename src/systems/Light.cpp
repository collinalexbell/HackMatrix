#include "systems/Light.h"
#include "components/Light.h"
#include "model.h"
#include "renderer.h"

void systems::updateLighting(
    std::shared_ptr<EntityRegistry> registry, Renderer* renderer) {
  auto view = registry->view<Light, Positionable>();
  std::function<void()> render = std::bind(&Renderer::render, renderer, LIGHT);
  for(auto [entity, light, positionable]: view.each()) {
    light.renderDepthMap(positionable.pos, render);
  }
}

