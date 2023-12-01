#pragma once
#include "chunk.h"
#include <memory>
#include "logger.h"

class Mesher {
  std::shared_ptr<spdlog::logger> logger;
public:
  Mesher();
  ChunkMesh meshGreedy(Chunk *chunk);
};
