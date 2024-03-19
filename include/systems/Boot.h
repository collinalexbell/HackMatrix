#pragma once

#include "entity.h"
#include <memory>
namespace systems {
  void boot(std::shared_ptr<EntityRegistry> registry,
            entt::entity entity,
            char** envp);
  void bootAll(std::shared_ptr<EntityRegistry> registry, char** envp);
  void killBootablesOnExit(std::shared_ptr<EntityRegistry>);
}
