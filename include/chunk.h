#pragma once
#include "cube.h"

class Chunk {
  Cube* data[128*128*128];
  Cube null = Cube(glm::vec3(0, 0, 0), -1);

  int index(int x, int y, int z);

public:
  Chunk();
  Cube *getCube(int x, int y, int z);
  void removeCube(int x, int y, int z);
  void addCube(Cube c, int x, int y, int z);
};
