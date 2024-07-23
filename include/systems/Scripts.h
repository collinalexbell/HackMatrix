#pragma once

#include "components/Scriptable.h"
#include "entity.h"
#include <memory>

namespace systems {
void editScript(std::shared_ptr<EntityRegistry>, entt::entity);
}
