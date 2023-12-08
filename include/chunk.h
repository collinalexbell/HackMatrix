#pragma once
#include "cube.h"
#include "coreStructs.h"
#include <memory>
#include <vector>
#include "mesher.h"


// TODO: rename, this is coords in chunk
struct ChunkCoords {
  int x;
  int y;
  int z;
};

struct ChunkPosition {
  int x;
  int y;
  int z;
};

class Chunk {
  int posX,posY,posZ;
  static int findNeighborFaceIndex(Face face);
  const vector<int> size = {32, 128, 32};
  unique_ptr<Cube* []> data;
  Cube null = Cube(glm::vec3(0, 0, 0), -1);
  int index(int x, int y, int z);
  unique_ptr<Mesher> mesher;
  ChunkMesh cachedSimpleMesh;
  ChunkMesh cachedGreedyMesh;
  bool damagedSimple = true;
  bool damagedGreedy = true;
  void setDamaged();

  // shares data[]
  friend ChunkMesh Mesher::simpleMesh(Chunk *chunk);

public:
  Chunk();
  Chunk(int x, int y, int z);
  Cube *getCube(int x, int y, int z);
  Cube *getCube_(int x, int y, int z);
  void removeCube(int x, int y, int z);
  void addCube(Cube c, int x, int y, int z);
  ChunkCoords getCoords(int i);
  ChunkMesh mesh(bool realTime);
  ChunkMesh meshedFaceFromPosition(Position position);
  const vector<int> getSize();
  ChunkPosition getPosition();
};
