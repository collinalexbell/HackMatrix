#pragma once

#include "entity.h"
#include <memory>
class Renderer;
namespace systems {
void
updateAll(std::shared_ptr<EntityRegistry>, Renderer* renderer);
void update(std::shared_ptr<EntityRegistry>, entt::entity);
}
