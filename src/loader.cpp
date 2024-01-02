#include "loader.h"
#include "enkimi.h"
#include <vector>

Coordinate getMinecraftChunkPos(int matrixChunkX, int matrixChunkZ) {
  auto matrixChunkSize = Chunk::getSize();
  vector<int> minecraftChunkSize = {16,256,16};
  auto x = matrixChunkX*matrixChunkSize[0]/minecraftChunkSize[0];
  auto z = matrixChunkZ * matrixChunkSize[2] / minecraftChunkSize[2];
  return Coordinate{x, z};
}

Coordinate getRelativeMinecraftChunkPos(int minecraftChunkX, int minecraftChunkZ) {
  vector<int> regionSize = {32, 32};
  assert(ENKI_MI_REGION_CHUNKS_NUMBER == regionSize[0] * regionSize[1]);
  int x = abs(minecraftChunkX) % 32;
  int z = abs(minecraftChunkZ) % 32;
  if(minecraftChunkX < 0) {
    x = 31 - x;
  }
  if(minecraftChunkZ < 0) {
    z = 31 - z;
  }
  return Coordinate{x,z};
}

Coordinate getMinecraftRegion(int minecraftChunkX, int minecraftChunkZ) {
  vector<int> regionSize = {32, 32};
  assert(ENKI_MI_REGION_CHUNKS_NUMBER == regionSize[0]*regionSize[1]);
  int subtractorX = 0;
  int subtractorZ = 0;
  if(minecraftChunkX < 0) {
    subtractorX = 1;
  }
  if(minecraftChunkZ < 0) {
    subtractorZ = 1;
  }
  return Coordinate{
    minecraftChunkX/regionSize[0]-subtractorX,
    minecraftChunkZ/regionSize[0]-subtractorZ
  };
}

Coordinate getWorldChunkPosFromMinecraft(int minecraftChunkX, int minecraftChunkZ) {
  auto matrixChunkSize = Chunk::getSize();
  vector<int> minecraftChunkSize = {16,256,16};
  float xf = float(minecraftChunkX * minecraftChunkSize[0]) / float(matrixChunkSize[0]);
  if(xf < 0) {
    xf = floor(xf);
  }
  int x = xf;

  float zf = float(minecraftChunkZ * minecraftChunkSize[2]) / float(matrixChunkSize[2]);
  if(zf < 0) {
    zf = floor(zf);
  }
  int z = zf;
  return Coordinate{x, z};
}
