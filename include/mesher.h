#pragma once
#include <memory>
#include "logger.h"
#include <glm/glm.hpp>
#include <vector>
#include <future>

using namespace std;

class Chunk;
class Position;

enum Face
{
  LEFT,
  RIGHT,
  BOTTOM,
  TOP,
  FRONT,
  BACK
};

enum MESH_TYPE
{
  SIMPLE,
  GREEDY
};

class ChunkPartition
{
  Chunk* chunk;
  int _y, ySize;

public:
  ChunkPartition(Chunk* chunk, int y, int ySize);
  int y();
  array<int, 3> getSize();
};

class ChunkPartitioner
{
  unsigned int partitionHeight;

public:
  ChunkPartitioner(unsigned int partitionHeight);
  vector<ChunkPartition> partition(Chunk*);
  unsigned int getPartitionHeight() { return partitionHeight; }
};

struct ChunkMesh
{
  MESH_TYPE type;
  vector<glm::vec3> positions;
  vector<glm::vec2> texCoords;
  vector<int> blockTypes;
  vector<int> selects;
  bool updated = true;
};

typedef vector<shared_ptr<ChunkMesh>> PartitionedChunkMeshes;

class Mesher
{
  unsigned int DEFAULT_PARTITION_HEIGHT = 20;
  int chunkX, chunkZ;
  Chunk* chunk;
  bool damagedGreedy;
  bool damagedSimple;
  ChunkPartitioner partitioner = ChunkPartitioner(DEFAULT_PARTITION_HEIGHT);
  vector<bool> partitionsDamaged = vector<bool>(DEFAULT_PARTITION_HEIGHT, true);
  shared_future<PartitionedChunkMeshes> cachedGreedyMesh;
  static glm::vec2 texModels[6][6];
  static Face neighborFaces[6];
  static glm::vec3 faceModels[6][6];
  vector<glm::vec2> getTexCoordsFromFace(Face face);
  int findNeighborFaceIndex(Face face);
  vector<glm::vec3> getOffsetsFromFace(Face face);
  Face getFaceFromNormal(glm::vec3 normal);
  shared_ptr<ChunkMesh> mergePartitionedChunkMeshes(PartitionedChunkMeshes);

public:
  Mesher(Chunk* chunk, int chunkX, int chunkZ);
  ChunkMesh meshedFaceFromPosition(Position position);
  PartitionedChunkMeshes meshGreedy(Chunk* chunk);
  shared_ptr<ChunkMesh> simpleMesh(Chunk* chunk);
  shared_ptr<ChunkMesh> mesh();
  void meshAsync();
  void meshDamaged(array<int, 3> pos);
};
