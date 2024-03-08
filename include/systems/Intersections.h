#pragma once
#include "components/BoundingSphere.h"
#include "entity.h"
#include <memory>

namespace systems {
  BoundingSphere emplaceBoundingSphere(std::shared_ptr<EntityRegistry>, entt::entity);
}
