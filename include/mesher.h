#pragma once
#include "chunk.h"

typedef vector<glm::vec3> Mesh;

class Mesher {
 public:
  Mesher() {}
  Mesh mesh(Chunk* chunk);
};
