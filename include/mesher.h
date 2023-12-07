#pragma once
#include <memory>
#include "logger.h"
#include <glm/glm.hpp>
#include <vector>

using namespace std;

class Chunk;
class Position;

enum Face { LEFT, RIGHT, BOTTOM, TOP, FRONT, BACK };

enum MESH_TYPE {
  SIMPLE, GREEDY
};

struct ChunkMesh {
  MESH_TYPE type;
  vector<glm::vec3> positions;
  vector<glm::vec2> texCoords;
  vector<int> blockTypes;
  vector<int> selects;
};

class Mesher {
  static glm::vec2 texModels[6][6];
  static Face neighborFaces[6];
  static glm::vec3 faceModels[6][6];
  vector<glm::vec2> getTexCoordsFromFace(Face face);
  int findNeighborFaceIndex(Face face);
  vector<glm::vec3> getOffsetsFromFace(Face face);
  Face getFaceFromNormal(glm::vec3 normal);
public:
  ChunkMesh meshedFaceFromPosition(Chunk* chunk, Position position);
  ChunkMesh meshGreedy(int chunkX, int chunkZ, Chunk *chunk);
  ChunkMesh simpleMesh(int chunkX, int chunkZ, Chunk *chunk);
};
