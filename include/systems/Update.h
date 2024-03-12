#pragma once

#include "entity.h"
#include <memory>
namespace systems {
  void update(std::shared_ptr<EntityRegistry>, entt::entity);
}
