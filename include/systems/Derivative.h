#pragma once

#include "entity.h"
#include <memory>

namespace systems {
  void createDerivativeComponents(std::shared_ptr<EntityRegistry>);
}
