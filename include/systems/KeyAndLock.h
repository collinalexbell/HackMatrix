#pragma once

#include "entity.h"
namespace systems {
  void turnKey(std::shared_ptr<EntityRegistry>, entt::entity);
  void unturnKey(std::shared_ptr<EntityRegistry>, entt::entity);
}
