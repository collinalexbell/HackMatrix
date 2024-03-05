#pragma once
#include "Door.h"
#include <entt.hpp>
#include "entity.h"
#include <memory>

namespace systems {
  void openDoor(std::shared_ptr<EntityRegistry>, entt::entity);
  void closeDoor(std::shared_ptr<EntityRegistry> , entt::entity);
};
