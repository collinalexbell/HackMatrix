#pragma once
#include "cube.h"
#include <memory>
#include <vector>

class Chunk {
  const vector<int> size = {256, 128, 256};
  unique_ptr<Cube* []> data;
  Cube null = Cube(glm::vec3(0, 0, 0), -1);
  int index(int x, int y, int z);

public:
  Chunk();
  Cube *getCube(int x, int y, int z);
  void removeCube(int x, int y, int z);
  void addCube(Cube c, int x, int y, int z);
  const vector<int> getSize();
};
