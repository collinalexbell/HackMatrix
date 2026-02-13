#pragma once

#include <memory>
#include "entity.h"

namespace systems
{
void applyURDFTransforms(std::shared_ptr<EntityRegistry> registry);
}
