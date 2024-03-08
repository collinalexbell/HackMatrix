#pragma once

#include "components/Scriptable.h"
#include "entity.h"
#include <memory>

namespace systems {
  void editor(std::filesystem::path);
  void editScript(std::shared_ptr<EntityRegistry>, entt::entity);
  void runScript(std::shared_ptr<EntityRegistry>, entt::entity);
}
