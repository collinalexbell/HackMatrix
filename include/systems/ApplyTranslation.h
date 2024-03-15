
#pragma once

#include "entity.h"
#include <memory>

namespace systems {
  void applyTranslations(std::shared_ptr<EntityRegistry>);
};
