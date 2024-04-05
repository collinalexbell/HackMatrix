#pragma once
#include "entity.h"
#include <memory>

class Renderer;
namespace systems {
  void updateLighting(std::shared_ptr<EntityRegistry>, Renderer *renderer);
}
