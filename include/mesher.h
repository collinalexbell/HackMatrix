#pragma once
#include <memory>
#include "logger.h"
#include <glm/glm.hpp>
#include <vector>

using namespace std;

class Chunk;

struct ChunkMesh {
  vector<glm::vec3> positions;
  vector<glm::vec2> texCoords;
  vector<int> blockTypes;
  vector<int> selects;
};

class Mesher {
  static std::shared_ptr<spdlog::logger> logger;
public:
  Mesher();
  ChunkMesh meshGreedy(int chunkX, int chunkZ, Chunk *chunk);
};
