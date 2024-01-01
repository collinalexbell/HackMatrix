#pragma once

#include "chunk.h"
#include <deque>

using namespace std;

namespace preload {
enum SIDE { LEFT, RIGHT };
};

struct ChunkIndex {
  bool isValid;
  int x;
  int z;
};

struct OrthoginalPreload {
  bool addToFront;
  deque<deque<Chunk *>> &chunks;
};

enum DIRECTION { NORTH, SOUTH, EAST, WEST };

struct Coordinate {
  int x;
  int z;

  Coordinate(array<int, 2> coords) {
    x = coords[0];
    z = coords[1];
  }

  Coordinate(int x, int z) : x(x), z(z) {}

  bool operator==(const Coordinate &other) const {
    return x == other.x && z == other.z;
  }
};

struct CoordinateHash {
  size_t operator()(const Coordinate &coordinate) const {
    return std::hash<int>()(coordinate.x) ^
           (std::hash<int>()(coordinate.z) << 1);
  }
};

class Loader {
  unordered_map<Coordinate, string, CoordinateHash> regionFiles;
  array<ChunkPosition, 2> getNextPreloadedChunkPositions(DIRECTION direction,
                                                         bool initial = false);

  OrthoginalPreload orthoginalPreload(DIRECTION direction, preload::SIDE side);
  ChunkIndex getChunkIndex(int x, int z);
  ChunkIndex playersChunkIndex();
  ChunkIndex calculateMiddleIndex();
};
