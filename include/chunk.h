#pragma once
#include "cube.h"
#include "coreStructs.h"
#include <memory>
#include <vector>


struct ChunkCoords {
  int x;
  int y;
  int z;
};

enum Face { LEFT, RIGHT, BOTTOM, TOP, FRONT, BACK };
struct ChunkMesh {
  vector<glm::vec3> positions;
  vector<glm::vec2> texCoords;
  vector<int> blockTypes;
  vector<int> selects;
};

class Chunk {
  static Face neighborFaces[6];
  static int findNeighborFaceIndex(Face face);
  const vector<int> size = {512, 128, 512};
  unique_ptr<Cube* []> data;
  Cube null = Cube(glm::vec3(0, 0, 0), -1);
  int index(int x, int y, int z);
  vector<glm::vec3> getOffsetsFromFace(Face face);
  vector<glm::vec2> getTexCoordsFromFace(Face face);
  Face getFaceFromNormal(glm::vec3 normal);

public:
  Chunk();
  Cube *getCube(int x, int y, int z);
  Cube *getCube_(int x, int y, int z);
  void removeCube(int x, int y, int z);
  void addCube(Cube c, int x, int y, int z);
  ChunkCoords getCoords(int i);
  ChunkMesh mesh();
  ChunkMesh meshedFaceFromPosition(Position position);
  const vector<int> getSize();
};
