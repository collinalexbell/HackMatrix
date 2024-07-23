#pragma once

#include "entity.h"
#include <memory>
#include <utility>
#include "app.h"
namespace systems {
void
boot(std::shared_ptr<EntityRegistry> registry,
     entt::entity entity,
     char** envp);
void
bootAll(std::shared_ptr<EntityRegistry> registry, char** envp);

std::vector<std::pair<entt::entity, int>>
getAlreadyBooted(std::shared_ptr<EntityRegistry> registry);

void killBootablesOnExit(std::shared_ptr<EntityRegistry>);
void
resizeBootable(std::shared_ptr<EntityRegistry>,
               entt::entity,
               int width,
               int height);
optional<entt::entity>
matchApp(shared_ptr<EntityRegistry>, X11App*);
}
