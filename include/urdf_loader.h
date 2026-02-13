#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <vector>
#include "entity.h"

struct URDFLoadResult
{
  std::vector<entt::entity> entities;
  std::vector<entt::entity> roots;
};

bool
loadURDFFromFile(std::shared_ptr<EntityRegistry> registry,
                 const std::string& urdfPath,
                 const glm::vec3& basePosition,
                 URDFLoadResult& outResult,
                 std::string& outError);
