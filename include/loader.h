#pragma once

#include "chunk.h"
#include "enkimi.h"
#include <deque>
#include <future>
#include <memory>
#include "blocks.h"

using namespace std;

namespace preload {
enum SIDE
{
  LEFT,
  RIGHT
};
};

struct ChunkIndex
{
  bool isValid;
  int x;
  int z;
};

struct OrthoginalPreload
{
  bool towardFront;
  bool leftToRight;
  deque<future<deque<shared_ptr<Chunk>>>>& chunks;
};

enum DIRECTION
{
  NORTH,
  SOUTH,
  EAST,
  WEST
};

struct Coordinate
{
  int x;
  int z;

  Coordinate(array<int, 2> coords)
  {
    x = coords[0];
    z = coords[1];
  }

  Coordinate(int x, int z)
    : x(x)
    , z(z)
  {
  }

  bool operator==(const Coordinate& other) const
  {
    return x == other.x && z == other.z;
  }
};

struct CoordinateHash
{
  size_t operator()(const Coordinate& coordinate) const
  {
    return std::hash<int>()(coordinate.x) ^
           (std::hash<int>()(coordinate.z) << 1);
  }
};

struct LoaderCube : public AbsolutePosition
{
  int blockType;
};

struct LoaderChunk
{
  int foreignChunkX, foreignChunkY, foreignChunkZ;
  vector<LoaderCube> cubePositions;
};

Coordinate
getMinecraftChunkPos(int matrixChunkX, int matrixChunkZ);
Coordinate
getRelativeMinecraftChunkPos(int minecraftChunkX, int minecraftChunkZ);
Coordinate
getMinecraftRegion(int minecraftChunkX, int minecraftChunkZ);
Coordinate
getWorldChunkPosFromMinecraft(int minecraftChunkX, int minecraftChunkZ);

// TODO: these are impl details. rm after full extractiton from world.cpp
std::array<int, 2>
getCoordinatesFromRegionFilename(const std::string& filename);
std::vector<std::string>
getFilesInFolder(const std::string& folderPath);

class Loader
{
  /*
  unordered_map<Coordinate, string, CoordinateHash> regionFiles;
  array<ChunkPosition, 2> getNextPreloadedChunkPositions(DIRECTION
direction, bool initial = false);

  OrthoginalPreload orthoginalPreload(DIRECTION direction, preload::SIDE
side); ChunkIndex getChunkIndex(int x, int z); ChunkIndex
playersChunkIndex(); ChunkIndex calculateMiddleIndex();
*/
  shared_ptr<blocks::TexturePack> texturePack;
  unordered_map<Coordinate, string, CoordinateHash> regionFileNames;
  unordered_map<Coordinate, enkiRegionFile, CoordinateHash> regionFiles;

public:
  Loader(string folderName, shared_ptr<blocks::TexturePack>);
  vector<LoaderChunk> getRegion(Coordinate regionCoordinate);
  future<deque<shared_ptr<Chunk>>> readNextChunkDeque(
    array<Coordinate, 2> chunkCoords);
};
