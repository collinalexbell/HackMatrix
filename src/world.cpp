#include "world.h"
#include "app.h"
#include "chunk.h"
#include "coreStructs.h"
#include "enkimi.h"
#include "glm/geometric.hpp"
#include "renderer.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <glm/glm.hpp>
#include <glm/gtx/intersect.hpp>
#include <iostream>
#include <limits>
#include <octree/octree.h>
#include <sstream>
#include <vector>

using namespace std;
namespace fs = std::filesystem;

World::World(Camera *camera, bool debug) : camera(camera) {
  initLogger();
  initAppPositions();
  initChunks();
}

void World::initLogger() {
  logger = make_shared<spdlog::logger>("World", fileSink);
  logger->set_level(spdlog::level::debug);
}

void World::initAppPositions() {
  float z = 10.0;
  availableAppPositions.push(glm::vec3(5.0, 9.0, z));
  availableAppPositions.push(glm::vec3(6.2, 9.0, z));
  availableAppPositions.push(glm::vec3(6.2, 9.75, z));
  availableAppPositions.push(glm::vec3(6.2, 10.50, z));
  availableAppPositions.push(glm::vec3(7.4, 9.0, z));
  availableAppPositions.push(glm::vec3(4.7, 9.75, z));
  availableAppPositions.push(glm::vec3(7.4, 9.75, z));
}

void World::initChunks() {
  assert(WORLD_SIZE % 2 == 1);

  int xMin = -1*WORLD_SIZE/2; int xMax = WORLD_SIZE/2;
  int zMin = -1*WORLD_SIZE/2; int zMax = WORLD_SIZE/2;

  assert(abs(xMin) == xMax);
  assert(abs(zMin) == zMax);
  for (int x = xMin; x <= xMax; x++) {
    chunks.push_back(deque<Chunk *>());
    for (int z = zMin; z <= zMax; z++) {
      chunks.back().push_back(new Chunk(x, 0, z));
    }
  }

  preloadVectors[NORTH] = {0,1};
  preloadVectors[SOUTH] = {0, -1};
  preloadVectors[EAST] = {1, 0};
  preloadVectors[WEST] = {-1, 0};

  // NORTH
  preloadedChunks[NORTH] = deque<deque<Chunk *>>();
  for(int z = zMax+1; z<zMax+1+PRELOAD_SIZE; z++) {
    deque<Chunk*> oneZ;
    for(int x = xMin; x <= xMax; x++) {
      oneZ.push_back(new Chunk(x,0,z));
    }
    preloadedChunks[NORTH].push_back(oneZ);
  }

  // SOUTH
  preloadedChunks[SOUTH] = deque<deque<Chunk *>>();
  for (int z = zMin - 1; z > zMin - 1 - PRELOAD_SIZE; z--) {
    deque<Chunk *> oneZ;
    for (int x = xMin; x <= xMax; x++) {
      oneZ.push_back(new Chunk(x, 0, z));
    }
    preloadedChunks[SOUTH].push_back(oneZ);
  }

  // EAST
  preloadedChunks[EAST] = deque<deque<Chunk *>>();
  for (int x = xMax + 1; x < xMax + 1 + PRELOAD_SIZE; x++) {
    deque<Chunk *> oneX;
    for (int z = zMin; z <= zMax; z++) {
      oneX.push_back(new Chunk(x, 0, z));
    }
    preloadedChunks[EAST].push_back(oneX);
  }

  // WEST
  preloadedChunks[WEST] = deque<deque<Chunk *>>();
  for (int x = xMin - 1; x > xMin - 1 - PRELOAD_SIZE; x--) {
    deque<Chunk *> oneX;
    for (int z = zMin; z <= zMax; z++) {
      oneX.push_back(new Chunk(x, 0, z));
    }
    preloadedChunks[WEST].push_back(oneX);
  }

  middleIndex = calculateMiddleIndex();
}

World::~World() {}

void World::mesh(bool realTime) {
  double currentTime = glfwGetTime();
  vector<shared_ptr<ChunkMesh>> m;
  int sizeX = chunks[0][0]->getSize()[0];
  int sizeZ = chunks[0][0]->getSize()[2];
  for(int x = 0; x < chunks.size(); x++) {
    for(int z = 0; z < chunks[x].size(); z++) {
        m.push_back(chunks[x][z]->mesh(realTime));
    }
  }
  stringstream ss;
  ss << "time: " << glfwGetTime() - currentTime;
  logger->debug(ss.str());
  logger->flush();
  renderer->updateChunkMeshBuffers(m);
}

const vector<Cube*> World::getCubes() {
  if(chunks.size()>0 && chunks[0].size() > 0) {
    auto chunkSize = chunks[0][0]->getSize();
    return getCubes(0,0,0,
                    chunkSize[0]*chunks.size(),
                    chunkSize[1],
                    chunkSize[2]*chunks[0].size());
  }
  return vector<Cube*>{};
}

const std::vector<Cube*> World::getCubes(int _x1, int _y1, int _z1,
                                        int _x2, int _y2, int _z2) {
  int x1 = _x1 < _x2 ? _x1 : _x2;
  int x2 = _x1 < _x2 ? _x2 : _x1;
  int y1 = _y1 < _y2 ? _y1 : _y2;
  int y2 = _y1 < _y2 ? _y2 : _y1;
  int z1 = _z1 < _z2 ? _z1 : _z2;
  int z2 = _z1 < _z2 ? _z2 : _z1;

  vector<Cube*> rv;
  for (int x = x1; x < x2; x++) {
    for (int y = y1; y < y2; y++) {
      for (int z = z1; z < z2; z++) {
        Cube *cube = getCube(x, y, z);
        if (cube->blockType() != -1) {
          rv.push_back(cube);
        }
      }
    }
  }
  return rv;
}

const std::vector<glm::vec3> World::getAppCubes() {
  std::vector<glm::vec3> appCubeKeys(appCubes.size());
  for(auto kv: appCubes) {
    appCubeKeys[kv.second] = kv.first;
  }
  return appCubeKeys;
}

WorldPosition World::translateToWorldPosition(int x, int y, int z) {
  WorldPosition rv;
  auto chunkSize = chunks[0][0]->getSize();

  // x-------------
  if (x < 0) {
    int chunksXNegative = (x - chunkSize[0] + 1) / chunkSize[0];
    rv.chunkX = chunksXNegative;
    rv.x = x - (chunksXNegative * chunkSize[0]);
  }
  else {
    rv.x = x % chunkSize[0];
    rv.chunkX = x / chunkSize[0];
  }

  //y-------------
  rv.y = y;

  // z-------------
  if (z < 0) {
    int chunksZNegative = (z - chunkSize[2] + 1) / chunkSize[2];
    rv.chunkZ = chunksZNegative;
    rv.z = z - (chunksZNegative * chunkSize[2]);
  } else {
    rv.z = z % chunkSize[2];
    rv.chunkZ = z / chunkSize[2];
  }

  return rv;
}

ChunkIndex World::getChunkIndex(int x, int z) {
  ChunkPosition minChunkPosition = chunks[0][0]->getPosition();
  ChunkPosition maxChunkPosition = chunks.back().back()->getPosition();
  ChunkIndex index;
  index.x = x - minChunkPosition.x;
  index.z = z - minChunkPosition.z;
  if (index.x >= 0 && index.x < chunks.size() && index.z >= 0 &&
      index.z < chunks[0].size()) {
    index.isValid = true;
  } else {
    index.isValid = false;
  }

  return index;
}

void World::loadChunksIfNeccissary() {
  ChunkIndex curIndex = playersChunkIndex();
  if(curIndex.x < middleIndex.x) {

    // rm bumped preloadedChunk on opposite side
    // I could do this off thread
    deque<Chunk*> chunksToRm = preloadedChunks[EAST].back();
    for(Chunk* toRm: chunksToRm) {
      delete toRm;
    }
    preloadedChunks[EAST].pop_back();

    // transfer from chunks to preloaded (opposite side)
    preloadedChunks[EAST].push_front(chunks.back());
    chunks.pop_back();

    // transfer from preloaded to chunks
    chunks.push_front(preloadedChunks[WEST].front());
    preloadedChunks[WEST].pop_front();

    // add chunks to preloaded
    // this allocates, so I could do this off thread
    /*
    deque<Chunk *> toAdd;
    auto pos = chunks[0][0]->getPosition();
    auto size = chunks[0][0]->getSize();
    for(int i = 0; i < chunks[0].size(); i++) {
      toAdd.push_back(new Chunk(pos.x-1-preloadedChunks[WEST].size(), pos.y,
    pos.z+i));
    }
    preloadedChunks[WEST].push_back(toAdd);
    */

    loadNextPreloadedChunkDeque(WEST);
    //int sign = std::signbit(pos.z) ? -1 : 1;

    /*
    for(int i = 0; i<size[0]; i++) {
      for(int j = 0; j<size[2]*chunks[0].size(); j++) {
        addCube((pos.x-1)*size[0]+i,5,pos.z*size[2]+j,3);
      }
    }
    */

    mesh();
  }
  if(curIndex.x > middleIndex.x) {

    // rm bumped preloadedChunk on opposite side
    deque<Chunk *> chunksToRm = preloadedChunks[WEST].back();
    for (Chunk *toRm : chunksToRm) {
      delete toRm;
    }
    preloadedChunks[WEST].pop_back();

    // transfer from chunks to preloaded (opposite side)
    preloadedChunks[WEST].push_front(chunks.front());
    chunks.pop_front();

    // transfer from preloaded to chunks
    chunks.push_back(preloadedChunks[EAST].front());
    preloadedChunks[EAST].pop_front();

    deque<Chunk *> toAdd;
    auto pos = chunks.back()[0]->getPosition();
    auto size = chunks.back()[0]->getSize();
    for (int i = 0; i < chunks[0].size(); i++) {
      toAdd.push_back(new Chunk(pos.x+1+preloadedChunks[EAST].size(), pos.y, pos.z + i));
    }

    preloadedChunks[EAST].push_back(toAdd);

    /*
    int sign = std::signbit(pos.z) ? -1 : 1;
    for (int i = 0; i < size[0]; i++) {
      for (int j = 0; j < size[2] * chunks[0].size(); j++) {
        addCube(pos.x * size[0] + i, 5, pos.z * size[2] + j, 3);
      }
    }
    */

    mesh();
  }
  if(curIndex.z < middleIndex.z) {
    // to be implemented later
  }
  if(curIndex.z > middleIndex.z) {
    // to be implemented later
  }
}

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
  auto x = minecraftChunkX * minecraftChunkSize[0] / matrixChunkSize[0];
  auto z = minecraftChunkZ * minecraftChunkSize[2] / matrixChunkSize[2];
  return Coordinate{x, z};
}

// Comparison function to sort by smallest x, z
bool sortByXZ(Chunk* chunk1, Chunk* chunk2) {
  auto pos1 = chunk1->getPosition();
  auto pos2 = chunk2->getPosition();
  if (pos1.x != pos2.x) {
    return pos1.x < pos2.x;
  } else {
    return pos1.z < pos2.z;
  }
}

void World::logCoordinates(array<Coordinate,2> c, string label) {
  stringstream ss;
  ss << label << ": (("
     << c[0].x << "," << c[0].z
     << "),("
     << c[1].x << "," << c[1].z
     << "))";
  logger->critical(ss.str());
  logger->flush();
}

deque<Chunk *> World::readNextChunkDeque(array<Coordinate, 2> chunkCoords,
                                         array<Coordinate, 2> regionCoords) {

  int startX;
  int endX;
  int startZ;
  int endZ;

  if(regionCoords[0].x < regionCoords[1].x) {
    startX = regionCoords[0].x;
    endX = regionCoords[1].x;
  } else {
    startX = regionCoords[1].x;
    endX = regionCoords[0].x;
  }

  if (regionCoords[0].z < regionCoords[1].z) {
    startZ = regionCoords[0].z;
    endZ = regionCoords[1].z;
  } else {
    startZ = regionCoords[1].z;
    endZ = regionCoords[0].z;
  }

  stringstream regionsDebug;
  regionsDebug << "regionCoords: (("
               << startX << "," << startZ << "),"
               << "("  << endX << "," << endZ << "))" << endl;
  logger->debug(regionsDebug.str());
  logger->flush();

  int chunkStartX;
  int chunkEndX;
  int chunkStartZ;
  int chunkEndZ;

  if (chunkCoords[0].x < chunkCoords[1].x) {
    chunkStartX = chunkCoords[0].x;
    chunkEndX = chunkCoords[1].x;
  } else {
    chunkStartX = chunkCoords[1].x;
    chunkEndX = chunkCoords[0].x;
  }

  if (chunkCoords[0].z < chunkCoords[1].z) {
    chunkStartZ = chunkCoords[0].z;
    chunkEndZ = chunkCoords[1].z;
  } else {
    chunkStartZ = chunkCoords[1].z;
    chunkEndZ = chunkCoords[0].z;
  }

  assert(startX == endX || startZ == endZ);

  logCoordinates({Coordinate{chunkStartX, chunkStartZ},
                  Coordinate{chunkEndX, chunkEndZ}},
                 "chunkStart");

  deque<Chunk*> nextChunkDeque;
  unordered_map < Coordinate, Chunk*, CoordinateHash> nextChunks;
  for (int x = startX; x <= endX; x++) {
    for(int z = startZ; z <= endZ; z++) {

      Coordinate regionCoords{x,z};

      auto region = getRegion(regionCoords);
      for(auto chunk: region) {
        if(chunk.foreignChunkX >= chunkStartX && chunk.foreignChunkX <= chunkEndX &&
           chunk.foreignChunkZ >= chunkStartZ && chunk.foreignChunkZ <= chunkEndZ) {
          auto worldChunkPos = getWorldChunkPosFromMinecraft(chunk.foreignChunkX, chunk.foreignChunkZ);

          if(!nextChunks.contains(worldChunkPos)) {
            Chunk *toAdd = new Chunk(worldChunkPos.x, 0, worldChunkPos.z);
            nextChunks[worldChunkPos] = toAdd;
          }
          for(auto cube: chunk.cubePositions){
            auto worldPos = translateToWorldPosition(cube.x, cube.y, cube.z);
            /*
              TODO: This fails, will need to be fixed
            assert(worldPos.chunkX == worldChunkPos.x &&
                   worldPos.chunkZ == worldChunkPos.z);
            */
            glm::vec3 pos {worldPos.x, worldPos.y, worldPos.z};
            Cube c(pos, cube.blockType);
            nextChunks[worldChunkPos]->addCube(c, worldPos.x, worldPos.y, worldPos.z);
          }
        }
      }
    }
  }

  for(auto nextChunk: nextChunks) {
    nextChunkDeque.push_back(nextChunk.second);
  }

  std::sort(nextChunkDeque.begin(), nextChunkDeque.end(), sortByXZ);

  return nextChunkDeque;
}

OrthoginalPreload World::orthoginalPreload(DIRECTION direction, preload::SIDE side) {
  switch(direction) {
  case NORTH:
    if(side == preload::LEFT) {
      return OrthoginalPreload{true, preloadedChunks[WEST]};
    } else {
      return OrthoginalPreload{true, preloadedChunks[EAST]};
    }
  case SOUTH:
    if(side == preload::LEFT) {
      return OrthoginalPreload{false, preloadedChunks[EAST]};
    } else {
      return OrthoginalPreload {false, preloadedChunks[WEST]};
    }
  case EAST:
    if(side == preload::LEFT) {
      return OrthoginalPreload{false, preloadedChunks[NORTH]};
    } else {
      return OrthoginalPreload{false, preloadedChunks[SOUTH]};
    }
  case WEST:
    if(side == preload::LEFT) {
      return OrthoginalPreload{true, preloadedChunks[SOUTH]};
    } else {
      return OrthoginalPreload{true, preloadedChunks[NORTH]};
    }
  }
  return OrthoginalPreload{true, preloadedChunks[NORTH]};
}

void World::loadNextPreloadedChunkDeque(DIRECTION direction) {
  auto matrixChunkPositions = getNextPreloadedChunkPositions(direction);
  // this needs to account for preload edges and doesn't currently
  // the reason is if I preload some WEST and then move NORTH...
  // there will be some NORTH that hasn't been preloaded

  int minecraftChunkSize = 16;
  int myChunkSize = 32;
  int minecraftPerMine = myChunkSize / minecraftChunkSize;
  // subtract 1 because we already have the first accounted for (we want the
  // end);
  int endAddition = minecraftPerMine - 1;

  array<Coordinate, 2> minecraftChunkPositions = {
    getMinecraftChunkPos(matrixChunkPositions[0].x, matrixChunkPositions[0].z),
    getMinecraftChunkPos(matrixChunkPositions[1].x, matrixChunkPositions[1].z)
  };
  minecraftChunkPositions[1].x += endAddition;
  minecraftChunkPositions[1].z += endAddition;

  array<Coordinate, 2> minecraftRegions = {
    getMinecraftRegion(minecraftChunkPositions[0].x, minecraftChunkPositions[0].z),
    getMinecraftRegion(minecraftChunkPositions[1].x, minecraftChunkPositions[1].z)
  };

  stringstream positionDebugStream;
  positionDebugStream << "matrixChunk: (("
                      << matrixChunkPositions[0].x
                      << ","
                      << matrixChunkPositions[0].z
                      << "),("
                      << matrixChunkPositions[1].x
                      << ","
                      << matrixChunkPositions[1].z
                      << "))"
                      << endl
                      << "minecraftChunk: (("
                      << minecraftChunkPositions[0].x
                      << ","
                      << minecraftChunkPositions[0].z
                      << "),("
                      << minecraftChunkPositions[1].x
                      << ","
                      << minecraftChunkPositions[1].z
                      << "))"
                      << endl
                      << "minecraftRegions: (("
                      << minecraftRegions[0].x
                      << ","
                      << minecraftRegions[0].z
                      << "),("
                      << minecraftRegions[1].x
                      << ","
                      << minecraftRegions[1].z
                      << "))"
                      << endl;

  logger->debug(positionDebugStream.str());
  logger->flush();

  auto next = readNextChunkDeque(minecraftChunkPositions, minecraftRegions);
  stringstream sizeSS;
  sizeSS << "alreadyLoaded:"<< preloadedChunks[direction][0].size()+2*PRELOAD_SIZE << ", next:" << next.size();
  logger->critical(sizeSS.str());


  assert(next.size() > PRELOAD_SIZE);
  preloadedChunks[direction].push_back(deque<Chunk*>(next.begin()+PRELOAD_SIZE,
                                                     next.end()-PRELOAD_SIZE));

  // sides
  auto leftChunks = deque<Chunk*>(next.begin(), next.begin()+PRELOAD_SIZE);
  OrthoginalPreload preloadLeft = orthoginalPreload(direction, preload::LEFT);
  int i = 0;
  for(auto toAdd = leftChunks.rbegin(); toAdd != leftChunks.rend(); toAdd++) {
    // iterate front to back of preload deque
    // (which is why reverse iterator in outerloop)
    if(preloadLeft.addToFront) {
      preloadLeft.chunks[i].push_front(*toAdd);
    } else {
      preloadLeft.chunks[i].push_back(*toAdd);
    }
    i++;
  }

  auto rightChunks = deque<Chunk *>(next.end()-PRELOAD_SIZE, next.end());
  OrthoginalPreload preloadRight = orthoginalPreload(direction, preload::RIGHT);
  i = 0;
  for (auto toAdd = rightChunks.begin(); toAdd != rightChunks.end(); toAdd++) {
    // front to back of preload is the same orientation as right, outerloop std iterator
    if (preloadRight.addToFront) {
      preloadRight.chunks[i].push_front(*toAdd);
    } else {
      preloadRight.chunks[i].push_back(*toAdd);
    }
    i++;
  }
}

array<ChunkPosition, 2> World::getNextPreloadedChunkPositions(DIRECTION direction) {
  int xAddition = 0, zAddition = 0;
  int xExpand=0, zExpand=0;
  switch (direction) {
  case WEST:
    xAddition = -1;
    zExpand = PRELOAD_SIZE;
    break;
  case EAST:
    xAddition = 1;
    zExpand = PRELOAD_SIZE;
    break;
  case NORTH:
    zAddition = 1;
    xExpand = PRELOAD_SIZE;
    break;
  case SOUTH:
    zAddition = -1;
    xExpand = PRELOAD_SIZE;
    break;
  }

  // TODO: fix this off by one error when getting these positions
  array<ChunkPosition, 2> positions = {
    // Lets make sure these are sorted properly...
    // the front should be the smallest...
    // at least according to initChunks
    preloadedChunks[direction].back().front()->getPosition(),
    preloadedChunks[direction].back().back()->getPosition()
  };

  positions[0].x += xAddition;
  positions[0].x -= xExpand;

  positions[0].z += zAddition;
  positions[0].z -= zExpand;


  positions[1].x += xAddition;
  positions[1].x += xExpand;

  positions[1].z += zAddition;
  positions[1].z += zExpand;

  return positions;
}

ChunkIndex World::calculateMiddleIndex() {
  ChunkIndex middleIndex;
  // even, make a choice, left or right (no middle)
  //20/2 = 10;
  // odd, index is clearly correct
  //21/2 = 10;

  middleIndex.x = chunks.size()/2;
  middleIndex.z = chunks[0].size()/2;
  middleIndex.isValid = true;
  return middleIndex;
}

ChunkIndex World::playersChunkIndex() {
  glm::vec3 voxelSpace = cameraToVoxelSpace(camera->position);
  auto worldPosition = translateToWorldPosition(voxelSpace.x, voxelSpace.y, voxelSpace.z);
  ChunkIndex rv = getChunkIndex(worldPosition.chunkX, worldPosition.chunkZ);
  return rv;
}


Chunk *World::getChunk(int x, int z) {
  ChunkIndex index = getChunkIndex(x, z);
  if(index.isValid) {
    return chunks[index.x][index.z];
  }
  return NULL;
}

void World::addCube(int x, int y, int z, int blockType) {
  WorldPosition pos = translateToWorldPosition(x, y, z);
  removeCube(pos);
  if (blockType >= 0) {
    glm::vec3 positionInChunk(pos.x, pos.y, pos.z);
    Cube cube(positionInChunk, blockType);
    Chunk *chunk = getChunk(pos.chunkX, pos.chunkZ);
    if (chunk != NULL) {
      chunk->addCube(cube, pos.x, pos.y, pos.z);
    }
  }
}

void World::addLine(Line line) {
  if(line.color.r >= 0) {
    int i = lines.size();
    lines.push_back(line);
    stringstream ss;
    ss << "adding line (" << i << ")"
       << line.points[0].x << ","
       << line.points[0].y << ","
       << line.points[0].z << ","
       << line.points[1].x << ","
       << line.points[1].y << ","
       << line.points[1].z << ","
       << line.color.r;
    logger->critical(ss.str());
    logger->flush();
    if(renderer != NULL) {
      renderer->addLine(i, line);
    }
  } else {
    removeLine(line);
  }
}

void World::refreshRendererCubes() {
  vector<glm::vec3> appCubesV = getAppCubes();
  for(int i=0; i < appCubesV.size(); i++){
    renderer->addAppCube(i, appCubesV[i]);
  }
}

void World::updateDamage(int index) {
  bool greaterDamage = !isDamaged || index < damageIndex;
  if (greaterDamage) {
    isDamaged = true;
    damageIndex = index;
  }
}

void World::removeCube(WorldPosition pos) {
  Chunk* chunk = getChunk(pos.chunkX, pos.chunkZ);
  if(chunk != NULL) {
    Cube *c = chunk->getCube_(pos.x,pos.y,pos.z);
    if(c != NULL) {
      chunk->removeCube(pos.x, pos.y, pos.z);
    }
  }
}

void World::removeLine(Line l) {
  float EPSILON = 0.001;
  for(auto it = lines.begin(); it != lines.end(); it++) {
    glm::vec3 a0 = it->points[0];
    glm::vec3 b0 = it->points[1];
    glm::vec3 a1 = l.points[0];
    glm::vec3 b1 = l.points[1];
    if(glm::distance(a0,a1)<EPSILON && glm::distance(b0,b1)<EPSILON) {
      lines.erase(it);
    }
  }
}

void World::addApp(X11App* app) {
  if(!app->isAccessory()) {
    glm::vec3 pos = availableAppPositions.front();
    addApp(pos, app);
    if(availableAppPositions.size() > 1) {
      availableAppPositions.pop();
    }
  } else {
    /*
    int index = appCubes.size();
    apps.push_back(app);
    renderer->registerApp(app, index);
    */
  }
}

void World::addApp(glm::vec3 pos, X11App* app) {
  int index = appCubes.size();
  appCubes.insert(std::pair<glm::vec3, int>(pos, index));
  apps.push_back(app);
  if(renderer != NULL) {
    logger->info("registerApp()");
    renderer->registerApp(app, index);
    logger->info("addAppCube");
    renderer->addAppCube(index, pos);

    stringstream debugInfo;
    debugInfo << "index:" << index << ", pos:" << pos.x << "," << pos.y << "," << pos.z;
    logger->debug(debugInfo.str());
    logger->flush();
  }
}

void World::removeApp(X11App *app) {
  int index = -1;
  for(int i=0; i<apps.size();i++) {
    if(apps[i] == app) {
      index = i;
    }
  }

  if(index < 0) {
    return;
  }

  auto it =
      std::find_if(appCubes.begin(), appCubes.end(),
                   [index](const std::pair<glm::vec3, int> &element) {
                     return element.second == index;
                   });

  appCubes.erase(it);
  apps.erase(apps.begin() + index);
  for(auto appKV = appCubes.begin(); appKV != appCubes.end(); appKV++) {
    if(appKV->second > index){
      appKV->second--;
    }
  }
  renderer->deregisterApp(index);
  refreshRendererCubes();
}

void World::attachRenderer(Renderer* renderer){
  this->renderer = renderer;
  refreshRendererCubes();
}

Cube* World::getCube(float x, float y, float z) {
  if(chunks.size() > 0 && chunks[0].size() > 0) {
    WorldPosition pos = translateToWorldPosition(x,y,z);
    Chunk *chunk = getChunk(pos.chunkX, pos.chunkZ);
    Cube* rv = chunk->getCube_(pos.x, pos.y, pos.z);
    return rv;
  }
  return NULL;
}

glm::vec3 World::cameraToVoxelSpace(glm::vec3 cameraPosition) {
  glm::vec3 halfAVoxel(0.5);
  glm::vec3 rv = (cameraPosition / glm::vec3(CUBE_SIZE)) + halfAVoxel;
  return rv;
}

struct Intersection {
  glm::vec3 intersectionPoint;
  float dist;
};

Intersection intersectLineAndPlane(glm::vec3 linePos, glm::vec3 lineDir, glm::vec3 planePos) {
  Intersection intersection;
  glm::vec3 normLineDir = glm::normalize(lineDir);
  glm::intersectRayPlane(linePos, normLineDir, planePos, glm::vec3(0,0,1), intersection.dist);
  intersection.intersectionPoint = (normLineDir * intersection.dist) + linePos;
  return intersection;
}

X11App* World::getLookedAtApp(){
  float DIST_LIMIT = 1.5;
  float height = 0.74;
  float width = 1.0;
  for (glm::vec3 appPosition : getAppCubes()) {
    Intersection intersection = intersectLineAndPlane(camera->position, camera->front, appPosition);
    float minX = appPosition.x - (width / 3);
    float maxX = appPosition.x + (width / 3);
    float minY = appPosition.y - (height / 3);
    float maxY = appPosition.y + (height / 3);
    float x = intersection.intersectionPoint.x;
    float y = intersection.intersectionPoint.y;
    if(x>minX && x<maxX && y>minY && y<maxY && intersection.dist < DIST_LIMIT) {
      int index = appCubes.at(appPosition);
      X11App* app = apps[index];
      return app;
    }
  }
  return NULL;
}

Position World::getLookedAtCube() {
  Position rv;
  rv.valid = false;
  glm::vec3 voxelSpace = cameraToVoxelSpace(camera->position);

  int x = (int)floor(voxelSpace.x);
  int y = (int)floor(voxelSpace.y);
  int z = (int)floor(voxelSpace.z);


  int stepX = ( camera->front.x > 0) ? 1 : -1;
  int stepY = ( camera->front.y > 0) ? 1 : -1;
  int stepZ = ( camera->front.z > 0) ? 1 : -1;

  // index<> already represents boundary if step<> is negative
  // otherwise add 1
  float tilNextX = x + ((stepX == 1) ? 1 : 0) - (voxelSpace.x); // voxelSpace, because float position
  float tilNextY = y + ((stepY == 1) ? 1 : 0) - (voxelSpace.y);
  float tilNextZ = z + ((stepZ == 1) ? 1 : 0) - (voxelSpace.z);
  // what happens if x is negative though...


  float tMaxX = camera->front.x != 0 ?
    tilNextX / camera->front.x :
    std::numeric_limits<float>::infinity();

  float tMaxY = camera->front.y != 0 ?
    tilNextY / camera->front.y :
    std::numeric_limits<float>::infinity();

  float tMaxZ = camera->front.z != 0 ?
    tilNextZ / camera->front.z :
    std::numeric_limits<float>::infinity();


  float tDeltaX = camera->front.x != 0 ?
    1 / abs(camera->front.x) :
    std::numeric_limits<float>::infinity();

  float tDeltaY = camera->front.y != 0 ?
    1 / abs(camera->front.y) :
    std::numeric_limits<float>::infinity();

  float tDeltaZ = camera->front.z != 0 ?
    1 / abs(camera->front.z) :
    std::numeric_limits<float>::infinity();


  int delta = 1;
  int limit = 20;

  glm::vec3 normal;
  glm::vec3 normalX = glm::vec3(stepX*-1, 0, 0);
  glm::vec3 normalY = glm::vec3(0, stepY*-1, 0);
  glm::vec3 normalZ = glm::vec3(0, 0, stepZ*-1);
  do {
    if(tMaxX < tMaxY) {
      if(tMaxX < tMaxZ){
        tMaxX = tMaxX + tDeltaX;
        x = x + stepX;
        normal = normalX;
      } else {
        tMaxZ = tMaxZ + tDeltaZ;
        z = z + stepZ;
        normal = normalZ;
      }
    } else {
      if(tMaxY < tMaxZ) {
        tMaxY = tMaxY + tDeltaY;
        y = y + stepY;
        normal = normalY;
      } else {
        tMaxZ = tMaxZ + tDeltaZ;
        z = z + stepZ;
        normal = normalZ;
      }
    }
    Cube *closest = getCube(x, y, z);
    if (closest != NULL) {
      rv.x = x;
      rv.y = y;
      rv.z = z;
      rv.normal = normal;
      rv.valid = true;
      return rv;
    }
  } while (tMaxX < limit || tMaxY < limit || tMaxZ < limit);

  return rv;
}

ChunkMesh World::meshSelectedCube(Position position) {
  if(chunks.size() > 0 && chunks[0].size() > 0) {
    WorldPosition worldPosition = translateToWorldPosition(position.x, position.y, position.z);
    Chunk *chunk = getChunk(worldPosition.chunkX, worldPosition.chunkZ);
    Position posInChunk{
      worldPosition.x, worldPosition.y, worldPosition.z,
      true, position.normal
    };
    return chunk->meshedFaceFromPosition(posInChunk);
  }
  return ChunkMesh{};
}

void World::action(Action toTake) {
  Position lookingAt = getLookedAtCube();
  if(lookingAt.valid) {
    Cube* lookedAt = getCube(lookingAt.x, lookingAt.y, lookingAt.z);
    if(toTake == PLACE_CUBE) {
      int x = lookingAt.x + (int)lookingAt.normal.x;
      int y = lookingAt.y + (int)lookingAt.normal.y;
      int z = lookingAt.z + (int)lookingAt.normal.z;
      addCube(x,y,z, lookedAt->blockType());
      mesh();
    }
    if(toTake == REMOVE_CUBE) {
      WorldPosition pos = translateToWorldPosition(lookingAt.x, lookingAt.y, lookingAt.z);
      removeCube(pos);
      mesh();
    }
    if(toTake == SELECT_CUBE) {
      lookedAt->toggleSelect();
    }
    if(toTake == OPEN_SELECTION_CODE) {
      logger->info("open_selection_code");
    }
  }
}

int World::getIndexOfApp(X11App *app) {
  for(int i = 0; i < apps.size(); i++) {
    if(app == apps[i]){
      return i;
    }
  }
  return -1;
}

float World::getViewDistanceForWindowSize(X11App *app) {
  // view = projection^-1 * gl_vertex * vertex^-1
  float glVertexX = float(app->width)/1920;
  glm::vec4 gl_pos = glm::vec4(10000,0,0,0);
  float zBest;
  float target = glVertexX;
    for (float z = 0.0; z <= 10.5; z = z + 0.001) {
      glm::vec4 candidate;
      candidate = renderer->projection * glm::vec4(0.5, 0, -z, 1);
      candidate = candidate/candidate.w;
      if(abs(candidate.x - target) < abs(gl_pos.x - target)) {
        gl_pos = candidate;
        zBest = z;
      }
    }
    return zBest;
}

glm::vec3 World::getAppPosition(X11App* app) {
  int index = -1;
  for(int i=0; i<apps.size(); i++) {
    if(app == apps[i]) {
      index = i;
    }
  }
  if(index == -1) {
    throw "app not found";
  }

  return getAppCubes()[index];

}

vector<Line> World::getLines() {
  return lines;
}

void World::save(string filename) {
  std::ofstream outputFile(filename);
  for(int chunkX = 0; chunkX < chunks.size(); chunkX++){
    for(int chunkZ = 0; chunkZ < chunks[0].size(); chunkZ++) {
      Chunk* chunk = chunks[chunkX][chunkZ];
      auto position = chunk->getPosition();
      auto size = chunk->getSize();
      for(int x=0; x<size[0]; x++) {
        for(int y=0; y<size[1]; y++) {
          for(int z=0; z<size[2]; z++) {
            Cube* cube = chunk->getCube_(x,y,z);
            if(cube != NULL) {
            outputFile << cube->position().x + size[0] * position.x << ","
                      << cube->position().y << ","
                      << cube->position().z + size[2] * position.z << ","
                      << cube->blockType() << endl;
            }
          }
        }
      }
    }
  }
  outputFile.close();
}

void World::load(string filename) {
  std::ifstream inputFile(filename);
  char comma;
  float x, y, z;
  int blockType;
  while (inputFile >> x >> comma >> y >> comma >> z >> comma >> blockType) {
    addCube(x,y,z,blockType);
  }
  inputFile.close();
  mesh();
}

std::vector<std::string> getFilesInFolder(const std::string &folderPath) {
  std::vector<std::string> files;

  for (const auto &entry : fs::directory_iterator(folderPath)) {
    if (fs::is_regular_file(entry.path())) {
      files.push_back(entry.path().filename().string());
    }
  }

  return files;
}

std::array<int, 2> getCoordinatesFromRegionFilename(const std::string &filename) {
  std::array<int, 2> coordinates = {
      0, 0}; // Initialize coordinates with default values

  try {
    // Extracting X and Z coordinates from the filename
    size_t startPos = filename.find_first_of(".") + 1;
    size_t endPos = filename.find_last_of(".");

    std::string coordsSubstring = filename.substr(startPos, endPos - startPos);
    size_t period = coordsSubstring.find_first_of(".");

    coordinates[0] = std::stoi(coordsSubstring.substr(0, period));
    coordinates[1] = std::stoi(coordsSubstring.substr(period + 1));
  } catch (const std::exception &e) {
    std::cerr << "Exception occurred: " << e.what() << std::endl;
    // Handle any exceptions, or you can leave the coordinates as default (0, 0)
  }

  return coordinates;
}

unsigned int getIndexIntoRegion(int x, int y, int z) {
  return (y * 16 + z) * 16 + x;
}


vector<LoaderChunk> World::getRegion(Coordinate regionCoordinate) {
  // copy contents of loadRegion except for addCube (which get converted to push_back())
  // in loadRegion, iterate over the chunks and cubes: call addCube()


  vector<LoaderChunk> chunks;

  map<int, int> counts;
  string path = regionFiles[regionCoordinate];
  FILE *fp = fopen(path.c_str(), "rb");
  enkiRegionFile regionFile = enkiRegionFileLoad(fp);
  logger->critical(path);
  logger->flush();
  for (unsigned int chunk = 0; chunk < ENKI_MI_REGION_CHUNKS_NUMBER; chunk++) {
    enkiNBTDataStream stream;
    enkiInitNBTDataStreamForChunk(regionFile, chunk, &stream);
    if(stream.dataLength) {
      enkiChunkBlockData aChunk = enkiNBTReadChunk(&stream);
      enkiMICoordinate chunkOriginPos = enkiGetChunkOrigin(&aChunk); // y always 0
      int chunkXPos = chunkOriginPos.x;
      int chunkZPos = chunkOriginPos.z;
      LoaderChunk lChunk;
      lChunk.foreignChunkX = chunkXPos / 16;
      lChunk.foreignChunkY = 0;
      lChunk.foreignChunkZ = chunkZPos / 16;
      chunks.push_back(lChunk);
      for (int section = 0; section < ENKI_MI_NUM_SECTIONS_PER_CHUNK; ++section) {
        if (aChunk.sections[section]) {
          enkiMICoordinate sectionOrigin = enkiGetChunkSectionOrigin(&aChunk, section);
          enkiMICoordinate sPos;
          for (sPos.y = 0; sPos.y < ENKI_MI_SIZE_SECTIONS; ++sPos.y) {
            for (sPos.z = 0; sPos.z < ENKI_MI_SIZE_SECTIONS; ++sPos.z) {
              for (sPos.x = 0; sPos.x < ENKI_MI_SIZE_SECTIONS; ++sPos.x) {
                uint8_t voxel =
                    enkiGetChunkSectionVoxel(&aChunk, section, sPos);
                if (voxel) {
                  if (!counts.contains(voxel)) {
                    counts[voxel] = 1;
                  } else {
                    counts[voxel]++;
                  }
                }
                LoaderCube cube;
                cube.x = sPos.x + sectionOrigin.x;
                cube.y = sPos.y + sectionOrigin.y;
                cube.z = sPos.z + sectionOrigin.z;
                bool shouldAdd = false;
                if (voxel == 1) {
                  cube.blockType = 6;
                  shouldAdd = true;
                }
                if(voxel == 3) {
                  cube.blockType = 3;
                  shouldAdd = true;
                }
                if(voxel == 161) {
                  cube.blockType = 0;
                  shouldAdd = true;
                }
                if(voxel == 251) {
                  cube.blockType = 1;
                  shouldAdd = true;
                }
                if (voxel == 17) {
                  cube.blockType = 2;
                  shouldAdd = true;
                }

                if(shouldAdd) {
                  chunks.back().cubePositions.push_back(cube);
                }
              }
            }
          }
        }
      }
    }
      //heights = reader.get_heightmap_at(x, z);
  }
  // Create a vector of pairs to sort by value (count)
  std::vector<std::pair<int, int>> sortedCounts(counts.begin(), counts.end());
  std::sort(sortedCounts.begin(), sortedCounts.end(),
            [](const auto &a, const auto &b) {
              return a.second > b.second; // Sort in descending order
            });
  int count = 0;
  for (const auto &entry : sortedCounts) {
    stringstream ss;
    ss << "Block ID: " << entry.first << ", Count: " << entry.second
              << std::endl;
    logger->critical(ss.str());
    logger->flush();
    count++;
    if (count == 6) // Stop after printing the top 6
      break;
  }

  return chunks;
}

void World::loadRegion(Coordinate regionCoordinate) {
  auto region = getRegion(regionCoordinate);
  for(auto chunk: region) {
    for(auto cube: chunk.cubePositions) {
      addCube(cube.x, cube.y, cube.z, cube.blockType);
    }
  }
}

void World::loadMinecraft(string folderName) {
  auto fileNames = getFilesInFolder(folderName);
  for(auto fileName: fileNames) {
    stringstream ss;
    auto coords = getCoordinatesFromRegionFilename(fileName);
    auto key = Coordinate(coords);
    regionFiles[key] = folderName + fileName;
  }
  loadRegion(Coordinate{0,0});
  mesh();
}

void World::loadLatest() {
  std::filesystem::path dirPath("saves");

  if (!std::filesystem::exists(dirPath) ||
      !std::filesystem::is_directory(dirPath)) {
    throw "Directory saves/ doesn't exist or is not a directory";
  }

  std::string latestSave;

  for (const auto &entry : std::filesystem::directory_iterator(dirPath)) {
    if (entry.is_regular_file()) {
      std::string filename = entry.path().filename().string();

      // Check if the file has a ".save" extension
      if (filename.size() >= 5 &&
          filename.substr(filename.size() - 5) == ".save") {
        if (filename > latestSave) {
          latestSave = filename;
        }
      }
    }
  }

  load("saves/" + latestSave);
}

void World::tick(){
  loadChunksIfNeccissary();
}
