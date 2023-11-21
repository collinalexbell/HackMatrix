#pragma once
#include "cube.h"
#include <memory>
#include <vector>

struct ChunkMesh {
  vector<glm::vec3> vertices;
  vector<int> ints;
};

struct ChunkCoords {
  int x;
  int y;
  int z;
};

class Chunk {
  const vector<int> size = {250, 128, 250};
  unique_ptr<Cube* []> data;
  Cube null = Cube(glm::vec3(0, 0, 0), -1);
  int index(int x, int y, int z);

public:
  Chunk();
  Cube *getCube(int x, int y, int z);
  Cube *getCube_(int x, int y, int z);
  void removeCube(int x, int y, int z);
  void addCube(Cube c, int x, int y, int z);
  ChunkCoords getCoords(int i);
  ChunkMesh mesh();
  const vector<int> getSize();
};
