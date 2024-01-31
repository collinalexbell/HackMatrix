#pragma once
#include <memory>
#include "logger.h"
#include <glm/glm.hpp>
#include <vector>
#include <future>

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
  bool updated = true;
};

class Mesher {
  int chunkX, chunkZ;
  Chunk* chunk;
  bool damagedGreedy;
  bool damagedSimple;
  shared_future<shared_ptr<ChunkMesh>> cachedGreedyMesh;
  static glm::vec2 texModels[6][6];
  static Face neighborFaces[6];
  static glm::vec3 faceModels[6][6];
  vector<glm::vec2> getTexCoordsFromFace(Face face);
  int findNeighborFaceIndex(Face face);
  vector<glm::vec3> getOffsetsFromFace(Face face);
  Face getFaceFromNormal(glm::vec3 normal);
public:
  Mesher(Chunk* chunk, int chunkX, int chunkZ);
  ChunkMesh meshedFaceFromPosition(Position position);
  shared_ptr<ChunkMesh> meshGreedy(Chunk* chunk);
  shared_ptr<ChunkMesh> simpleMesh(Chunk* chunk);
  shared_ptr<ChunkMesh> mesh();
  void meshAsync();
  void meshDamaged();
};
