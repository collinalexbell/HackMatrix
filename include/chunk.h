#pragma once
#include "cube.h"
#include "coreStructs.h"
#include <memory>
#include <vector>
#include "mesher.h"


struct ChunkCoords {
  int x;
  int y;
  int z;
};

enum Face { LEFT, RIGHT, BOTTOM, TOP, FRONT, BACK };
class Chunk {
  int posX,posY,posZ;
  static Face neighborFaces[6];
  static int findNeighborFaceIndex(Face face);
  const vector<int> size = {32, 128, 32};
  unique_ptr<Cube* []> data;
  Cube null = Cube(glm::vec3(0, 0, 0), -1);
  int index(int x, int y, int z);
  vector<glm::vec3> getOffsetsFromFace(Face face);
  vector<glm::vec2> getTexCoordsFromFace(Face face);
  Face getFaceFromNormal(glm::vec3 normal);
  unique_ptr<Mesher> mesher;
  ChunkMesh cachedSimpleMesh;
  ChunkMesh cachedGreedyMesh;
  bool damagedSimple = true;
  bool damagedGreedy = true;
  void setDamaged();
  ChunkMesh simpleMesh();

public:
  static glm::vec2 texModels[6][6];
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
};
