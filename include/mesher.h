#pragma once
#include "chunk.h"
#include <memory>
#include "logger.h"

typedef vector<glm::vec3> Mesh;

class Mesher {
  std::shared_ptr<spdlog::logger> logger;
public:
  Mesher();
  Mesh mesh(Chunk *chunk);
};
