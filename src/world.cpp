#include "world.h"
#include "app.h"
#include "chunk.h"
#include "components/BoundingSphere.h"
#include "coreStructs.h"
#include "enkimi.h"
#include "glm/geometric.hpp"
#include "loader.h"
#include "renderer.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <deque>
#include <exception>
#include <filesystem>
#include <fstream>
#include <glm/glm.hpp>
#include <glm/gtx/intersect.hpp>
#include <iostream>
#include <limits>
#include <octree/octree.h>
#include <sstream>
#include <vector>
#include "systems/ApplyTranslation.h"
#include "systems/Intersections.h"
#include "systems/Scripts.h"
#include "systems/Update.h"
#include "utility.h"
#include <csignal>
#include "memory.h"
#include "systems/ApplyRotation.h"
#include "tracy/Tracy.hpp"

using namespace std;

World::World(shared_ptr<EntityRegistry> registry,
             Camera* camera,
             shared_ptr<blocks::TexturePack> texturePack,
             string minecraftFolder,
             bool debug,
             spdlog::sink_ptr loggerSink)
  : registry(registry)
  , camera(camera)
{
  initLogger(loggerSink);
  logger->debug("Hello World!");
  // initLoader(minecraftFolder, texturePack);
  // initChunks();
  dynamicObjects = make_shared<DynamicObjectSpace>();
  dynamicCube = make_shared<DynamicCube>(glm::vec3(0.0f, 8.0f, 0.0f),
                                         glm::vec3(0.1f, 0.1f, 0.1f));
  dynamicObjects->addObject(dynamicCube);

  /* Shows how to init entity and components. Will need to do this with imgui
  auto npc = registry->createPersistent();
  registry->emplace<Model>(npc, "/home/collin/matrix/vox/hacker.obj");
  registry->emplace<Positionable>(npc, glm::vec3(0, 0.2, -1.0), 0.1);

  auto cave = registry->createPersistent();
  registry->emplace<Model>(cave, "/home/collin/matrix/vox/cave.obj");
  registry->emplace<Positionable>(cave, glm::vec3(0, 0, 0), 0.4);

  auto light = registry->createPersistent();
  registry->emplace<Model>(light, "/home/collin/matrix/vox/light.obj");
  registry->emplace<Positionable>(light, glm::vec3(-0.6, 1.5, 0), 0.1);
  registry->emplace<Light>(light, glm::vec3(1.0,1.0,1.0));
  */
}

void
World::initLogger(spdlog::sink_ptr loggerSink)
{
  logger = make_shared<spdlog::logger>("World", loggerSink);
  logger->set_level(spdlog::level::debug);
}

void
World::initChunks()
{
  assert(WORLD_SIZE % 2 == 1);

  int xMin = -1 * WORLD_SIZE / 2;
  int xMax = WORLD_SIZE / 2;
  int zMin = -1 * WORLD_SIZE / 2;
  int zMax = WORLD_SIZE / 2;

  assert(abs(xMin) == xMax);
  assert(abs(zMin) == zMax);
  for (int x = xMin; x <= xMax; x++) {
    chunks.push_back(deque<shared_ptr<Chunk>>());
    for (int z = zMin; z <= zMax; z++) {
      chunks.back().push_back(make_shared<Chunk>(x, 0, z));
    }
  }

  // SOUTH
  preloadedChunks[SOUTH] = deque<future<deque<shared_ptr<Chunk>>>>();
  for (int z = zMax + 1; z < zMax + 1 + PRELOAD_SIZE; z++) {
    loadNextPreloadedChunkDeque(SOUTH, true);
  }

  // NORTH
  preloadedChunks[NORTH] = deque<future<deque<shared_ptr<Chunk>>>>();
  for (int z = zMin - 1; z > zMin - 1 - PRELOAD_SIZE; z--) {
    loadNextPreloadedChunkDeque(NORTH, true);
  }

  // EAST
  preloadedChunks[EAST] = deque<future<deque<shared_ptr<Chunk>>>>();
  for (int x = xMax + 1; x < xMax + 1 + PRELOAD_SIZE; x++) {
    loadNextPreloadedChunkDeque(EAST, true);
  }

  // WEST
  preloadedChunks[WEST] = deque<future<deque<shared_ptr<Chunk>>>>();
  for (int x = xMin - 1; x > xMin - 1 - PRELOAD_SIZE; x--) {
    loadNextPreloadedChunkDeque(WEST, true);
  }

  middleIndex = calculateMiddleIndex();
}

World::~World() {}

void
World::mesh(bool realTime)
{
  double currentTime = glfwGetTime();
  vector<shared_ptr<ChunkMesh>> m;
  int sizeX = chunks[0][0]->getSize()[0];
  int sizeZ = chunks[0][0]->getSize()[2];
  for (int x = 0; x < chunks.size(); x++) {
    for (int z = 0; z < chunks[x].size(); z++) {
      m.push_back(chunks[x][z]->mesh());
    }
  }
  renderer->updateChunkMeshBuffers(m);
}

const vector<shared_ptr<Cube>>
World::getCubes()
{
  if (chunks.size() > 0 && chunks[0].size() > 0) {
    auto chunkSize = chunks[0][0]->getSize();
    return getCubes(0,
                    0,
                    0,
                    chunkSize[0] * chunks.size(),
                    chunkSize[1],
                    chunkSize[2] * chunks[0].size());
  }
  return vector<shared_ptr<Cube>>{};
}

const std::vector<shared_ptr<Cube>>
World::getCubes(int _x1, int _y1, int _z1, int _x2, int _y2, int _z2)
{
  int x1 = _x1 < _x2 ? _x1 : _x2;
  int x2 = _x1 < _x2 ? _x2 : _x1;
  int y1 = _y1 < _y2 ? _y1 : _y2;
  int y2 = _y1 < _y2 ? _y2 : _y1;
  int z1 = _z1 < _z2 ? _z1 : _z2;
  int z2 = _z1 < _z2 ? _z2 : _z1;

  vector<shared_ptr<Cube>> rv;
  for (int x = x1; x < x2; x++) {
    for (int y = y1; y < y2; y++) {
      for (int z = z1; z < z2; z++) {
        auto cube = getCube(x, y, z);
        if (cube->blockType() != -1) {
          rv.push_back(cube);
        }
      }
    }
  }
  return rv;
}

ChunkIndex
World::getChunkIndex(int x, int z)
{
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

DIRECTION
oppositeDirection(DIRECTION direction)
{
  switch (direction) {
    case NORTH:
      return SOUTH;
    case SOUTH:
      return NORTH;
    case EAST:
      return WEST;
    case WEST:
      return EAST;
  }
  return NORTH;
}
void
World::transferChunksToPreload(DIRECTION direction,
                               deque<shared_ptr<Chunk>> slice)
{
  OrthoginalPreload left =
    orthoginalPreload(oppositeDirection(direction), preload::LEFT);
  vector<shared_future<deque<shared_ptr<Chunk>>>> sharedLeft;
  for (auto& leftDeque : left.chunks) {
    sharedLeft.push_back(leftDeque.share());
  }
  OrthoginalPreload right =
    orthoginalPreload(oppositeDirection(direction), preload::RIGHT);
  vector<shared_future<deque<shared_ptr<Chunk>>>> sharedRight;
  for (auto& rightDeque : right.chunks) {
    sharedRight.push_back(rightDeque.share());
  }
  auto toPreload = async(
    launch::async,
    [slice,
     sharedLeft,
     sharedRight,
     towardFront = left.towardFront,
     leftToRight = left.leftToRight,
     PRELOAD_SIZE = this->PRELOAD_SIZE]() -> deque<shared_ptr<Chunk>> {
      deque<shared_ptr<Chunk>> rv;
      vector<deque<shared_ptr<Chunk>>> leftDeques;
      vector<deque<shared_ptr<Chunk>>> rightDeques;
      for (auto& sharedLeftDeque : sharedLeft) {
        leftDeques.push_back(sharedLeftDeque.get());
      }
      for (auto& sharedRightDeque : sharedRight) {
        rightDeques.push_back(sharedRightDeque.get());
      }
      if (leftToRight) {
        for (auto leftDeque = leftDeques.rbegin();
             leftDeque != leftDeques.rend();
             leftDeque++) {
          int index =
            towardFront ? leftDeque->size() - PRELOAD_SIZE - 1 : PRELOAD_SIZE;
          rv.push_back((*leftDeque)[index]);
        }
      } else {
        for (auto rightDeque = rightDeques.rbegin();
             rightDeque != rightDeques.rend();
             rightDeque++) {
          int index =
            towardFront ? rightDeque->size() - PRELOAD_SIZE - 1 : PRELOAD_SIZE;
          rv.push_back((*rightDeque)[index]);
        }
      }
      rv.insert(rv.end(), slice.begin(), slice.end());
      if (leftToRight) {
        for (auto rightDeque = rightDeques.begin();
             rightDeque != rightDeques.end();
             rightDeque++) {
          int index =
            towardFront ? rightDeque->size() - PRELOAD_SIZE - 1 : PRELOAD_SIZE;
          rv.push_back((*rightDeque)[index]);
        }
      } else {
        for (auto leftDeque = leftDeques.begin(); leftDeque != leftDeques.end();
             leftDeque++) {
          int index =
            towardFront ? leftDeque->size() - PRELOAD_SIZE - 1 : PRELOAD_SIZE;
          rv.push_back((*leftDeque)[index]);
        }
      }
      return rv;
    });
  // unshare
  for (int i = 0; i < sharedLeft.size(); i++) {
    left.chunks[i] = async(
      launch::async, [sharedLeft, i]() mutable -> deque<shared_ptr<Chunk>> {
        return sharedLeft[i].get();
      });
  }
  for (int i = 0; i < sharedRight.size(); i++) {
    right.chunks[i] = async(
      launch::async, [sharedRight, i]() mutable -> deque<shared_ptr<Chunk>> {
        return sharedRight[i].get();
      });
  }
  preloadedChunks[direction].push_front(move(toPreload));
}

void
World::loadChunksIfNeccissary()
{
  ChunkIndex curIndex = playersChunkIndex();
  if (curIndex.x < middleIndex.x) {
    preloadedChunks[EAST].pop_back();

    // transfer from chunks to preloaded (opposite side)
    transferChunksToPreload(EAST, chunks.back());
    chunks.pop_back();

    // transfer from preloaded to chunks
    auto full = preloadedChunks[WEST].front().get();
    deque<shared_ptr<Chunk>> toChunks(full.begin() + PRELOAD_SIZE,
                                      full.end() - PRELOAD_SIZE);
    chunks.push_front(toChunks);

    preloadedChunks[WEST].pop_front();
    loadNextPreloadedChunkDeque(WEST);
    mesh();
  }
  if (curIndex.x > middleIndex.x) {
    preloadedChunks[WEST].pop_back();

    // transfer from chunks to preloaded (opposite side)
    transferChunksToPreload(WEST, chunks.front());
    chunks.pop_front();

    // transfer from preloaded to chunks
    auto full = preloadedChunks[EAST].front().get();
    deque<shared_ptr<Chunk>> toChunks(full.begin() + PRELOAD_SIZE,
                                      full.end() - PRELOAD_SIZE);
    chunks.push_back(toChunks);
    preloadedChunks[EAST].pop_front();

    loadNextPreloadedChunkDeque(EAST);
    mesh();
  }
  if (curIndex.z < middleIndex.z) {
    stringstream ss;
    preloadedChunks[SOUTH].pop_back();

    // transfer from chunks to preloaded (opposite side)
    deque<shared_ptr<Chunk>> preloadSlice;
    for (auto& northSouthSlice : chunks) {
      preloadSlice.push_back(northSouthSlice.back());
      northSouthSlice.pop_back();
    }
    transferChunksToPreload(SOUTH, preloadSlice);

    // transfer from preloaded to chunks
    auto full = preloadedChunks[NORTH].front().get();
    deque<shared_ptr<Chunk>> westEastSlice(full.begin() + PRELOAD_SIZE,
                                           full.end() - PRELOAD_SIZE);
    int i = 0;
    for (auto& northSouthSlice : chunks) {
      northSouthSlice.push_front(westEastSlice[i]);
      i++;
    }
    preloadedChunks[NORTH].pop_front();
    loadNextPreloadedChunkDeque(NORTH);
    mesh();
  }
  if (curIndex.z > middleIndex.z) {
    preloadedChunks[NORTH].pop_back();
    // transfer from chunks to preloaded (opposite side)
    deque<shared_ptr<Chunk>> preloadSlice;
    for (auto& northSouthSlice : chunks) {
      preloadSlice.push_back(northSouthSlice.front());
      northSouthSlice.pop_front();
    }
    transferChunksToPreload(NORTH, preloadSlice);

    // transfer from preloaded to chunks
    auto full = preloadedChunks[SOUTH].front().get();
    deque<shared_ptr<Chunk>> westEastSlice(full.begin() + PRELOAD_SIZE,
                                           full.end() - PRELOAD_SIZE);
    int i = 0;
    for (auto& northSouthSlice : chunks) {
      northSouthSlice.push_back(westEastSlice[i]);
      i++;
    }
    preloadedChunks[SOUTH].pop_front();
    loadNextPreloadedChunkDeque(SOUTH);
    mesh();
  }
}

void
World::logCoordinates(array<Coordinate, 2> c, string label)
{
  stringstream ss;
  ss << label << ": ((" << c[0].x << "," << c[0].z << "),(" << c[1].x << ","
     << c[1].z << "))";
  logger->critical(ss.str());
  logger->flush();
}

OrthoginalPreload
World::orthoginalPreload(DIRECTION direction, preload::SIDE side)
{
  switch (direction) {
    case DIRECTION::NORTH:
      if (side == preload::LEFT) {
        return OrthoginalPreload{ true, true, preloadedChunks[WEST] };
      } else {
        return OrthoginalPreload{ true, true, preloadedChunks[EAST] };
      }
    case DIRECTION::SOUTH:
      if (side == preload::LEFT) {
        return OrthoginalPreload{ false, false, preloadedChunks[EAST] };
      } else {
        return OrthoginalPreload{ false, false, preloadedChunks[WEST] };
      }
    case DIRECTION::EAST:
      if (side == preload::LEFT) {
        return OrthoginalPreload{ false, true, preloadedChunks[NORTH] };
      } else {
        return OrthoginalPreload{ false, true, preloadedChunks[SOUTH] };
      }
    case DIRECTION::WEST:
      if (side == preload::LEFT) {
        return OrthoginalPreload{ true, false, preloadedChunks[SOUTH] };
      } else {
        return OrthoginalPreload{ true, false, preloadedChunks[NORTH] };
      }
  }
  return OrthoginalPreload{ true, true, preloadedChunks[NORTH] };
}

void
World::loadNextPreloadedChunkDeque(DIRECTION direction, bool isInitial)
{
  auto matrixChunkPositions = getNextPreloadedChunkPositions(
    direction, preloadedChunks[direction].size() + 1, isInitial);
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

  auto nextUnshared = loader->readNextChunkDeque(minecraftChunkPositions);

  if (isInitial) {
    preloadedChunks[direction].push_back(move(nextUnshared));
  } else {
    auto next = nextUnshared.share();
    OrthoginalPreload preloadLeft = orthoginalPreload(direction, preload::LEFT);
    for (int preloadIndex = 0; preloadIndex < PRELOAD_SIZE; preloadIndex++) {
      preloadLeft.chunks[preloadIndex] = async(
        launch::async,
        [PRELOAD_SIZE = this->PRELOAD_SIZE,
         preloadIndex,
         next,
         leftNext = move(preloadLeft.chunks[preloadIndex]),
         addToFront = preloadLeft.towardFront,
         leftToRight =
           preloadLeft.leftToRight]() mutable -> deque<shared_ptr<Chunk>> {
          auto origDeque = leftNext.get();
          // PRELOAD_SIZE-1-preloadIndex because...
          // On the left side, preload deque goes from right to left
          // whereas next always goes left to right
          auto loadedNext = next.get();
          int index = leftToRight
                        ? PRELOAD_SIZE - 1 - preloadIndex
                        : loadedNext.size() - PRELOAD_SIZE + preloadIndex;
          auto toAdd = loadedNext[index];
          if (addToFront) {
            origDeque.push_front(toAdd);
            origDeque.pop_back();
            // TODO: clean up with delete or smart pointer
          } else {
            origDeque.push_back(toAdd);
            origDeque.pop_front();
            // TODO: clean up with delete or smart pointer
          }
          return origDeque;
        });
    }

    OrthoginalPreload preloadRight =
      orthoginalPreload(direction, preload::RIGHT);
    for (int preloadIndex = 0; preloadIndex < PRELOAD_SIZE; preloadIndex++) {
      preloadRight.chunks[preloadIndex] = async(
        launch::async,
        [PRELOAD_SIZE = this->PRELOAD_SIZE,
         preloadIndex,
         next,
         rightNext = move(preloadRight.chunks[preloadIndex]),
         addToFront = preloadRight.towardFront,
         leftToRight =
           preloadRight.leftToRight]() mutable -> deque<shared_ptr<Chunk>> {
          auto origDeque = rightNext.get();
          auto loadedNext = next.get();
          int index = leftToRight
                        ? loadedNext.size() - PRELOAD_SIZE + preloadIndex
                        : PRELOAD_SIZE - 1 - preloadIndex;
          auto toAdd = loadedNext[index];
          if (addToFront) {
            origDeque.push_front(toAdd);
            origDeque.pop_back();
            // TODO: clean up with delete or smart pointer
          } else {
            origDeque.push_back(toAdd);
            origDeque.pop_front();
            // TODO: clean up with delete or smart pointer
          }
          return origDeque;
        });
    }
    preloadedChunks[direction].push_back(async(
      launch::async, [next, this, direction]() -> deque<shared_ptr<Chunk>> {
        return next.get();
      }));
  }
}

array<ChunkPosition, 2>
World::getNextPreloadedChunkPositions(DIRECTION direction,
                                      int nextPreloadCount,
                                      bool isInitial)
{
  int xAddition = 0, zAddition = 0;
  int xExpand = 0, zExpand = 0;
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
      zAddition = -1;
      xExpand = PRELOAD_SIZE;
      break;
    case SOUTH:
      zAddition = 1;
      xExpand = PRELOAD_SIZE;
      break;
  }

  array<ChunkPosition, 2> positions;
  int xIndex = -1, zIndex = -1;
  switch (direction) {
    case NORTH:
      zIndex = 0;
      break;
    case SOUTH:
      zIndex = chunks[0].size() - 1;
      break;
    case EAST:
      xIndex = chunks.size() - 1;
      break;
    case WEST:
      xIndex = 0;
      break;
  }
  if (xIndex >= 0) {
    positions = { chunks[xIndex].front()->getPosition(),
                  chunks[xIndex].back()->getPosition() };
  }
  if (zIndex >= 0) {
    positions = { chunks.front()[zIndex]->getPosition(),
                  chunks.back()[zIndex]->getPosition() };
  }
  positions[0].x -= xExpand;
  positions[0].z -= zExpand;
  positions[1].x += xExpand;
  positions[1].z += zExpand;
  positions[0].x += xAddition * nextPreloadCount;
  positions[0].z += zAddition * nextPreloadCount;
  positions[1].x += xAddition * nextPreloadCount;
  positions[1].z += zAddition * nextPreloadCount;

  return positions;
}

ChunkIndex
World::calculateMiddleIndex()
{
  ChunkIndex middleIndex;
  // even, make a choice, left or right (no middle)
  // 20/2 = 10;
  // odd, index is clearly correct
  // 21/2 = 10;

  middleIndex.x = chunks.size() / 2;
  middleIndex.z = chunks[0].size() / 2;
  middleIndex.isValid = true;
  return middleIndex;
}

ChunkIndex
World::playersChunkIndex()
{
  glm::vec3 voxelSpace = cameraToVoxelSpace(camera->position);
  auto worldPosition =
    translateToWorldPosition(voxelSpace.x, voxelSpace.y, voxelSpace.z);
  ChunkIndex rv = getChunkIndex(worldPosition.chunkX, worldPosition.chunkZ);
  return rv;
}

shared_ptr<Chunk>
World::getChunk(int x, int z)
{
  ChunkIndex index = getChunkIndex(x, z);
  if (index.isValid) {
    return chunks[index.x][index.z];
  }
  return NULL;
}

void
World::addCube(int x, int y, int z, int blockType)
{
  WorldPosition pos = translateToWorldPosition(x, y, z);
  removeCube(pos);
  if (blockType >= 0) {
    glm::vec3 positionInChunk(pos.x, pos.y, pos.z);
    Cube cube(positionInChunk, blockType);
    shared_ptr<Chunk> chunk = getChunk(pos.chunkX, pos.chunkZ);
    if (chunk != NULL) {
      chunk->addCube(cube, pos.x, pos.y, pos.z);
    }
  }
}

void
World::addLine(Line line)
{
  if (line.color.r >= 0) {
    int i = lines.size();
    lines.push_back(line);
    if (renderer != NULL) {
      renderer->addLine(i, line);
    }
  } else {
    removeLine(line);
  }
}

void
World::updateDamage(int index)
{
  bool greaterDamage = !isDamaged || index < damageIndex;
  if (greaterDamage) {
    isDamaged = true;
    damageIndex = index;
  }
}

void
World::removeCube(WorldPosition pos)
{
  shared_ptr<Chunk> chunk = getChunk(pos.chunkX, pos.chunkZ);
  if (chunk != NULL) {
    auto c = chunk->getCube_(pos.x, pos.y, pos.z);
    if (c != NULL) {
      chunk->removeCube(pos.x, pos.y, pos.z);
    }
  }
}

void
World::removeLine(Line l)
{
  float EPSILON = 0.001;
  for (auto it = lines.begin(); it != lines.end(); it++) {
    glm::vec3 a0 = it->points[0];
    glm::vec3 b0 = it->points[1];
    glm::vec3 a1 = l.points[0];
    glm::vec3 b1 = l.points[1];
    if (glm::distance(a0, a1) < EPSILON && glm::distance(b0, b1) < EPSILON) {
      lines.erase(it);
    }
  }
}

void
World::attachRenderer(Renderer* renderer)
{
  this->renderer = renderer;
}

shared_ptr<Cube>
World::getCube(float x, float y, float z)
{
  if (chunks.size() > 0 && chunks[0].size() > 0) {
    WorldPosition pos = translateToWorldPosition(x, y, z);
    shared_ptr<Chunk> chunk = getChunk(pos.chunkX, pos.chunkZ);
    auto rv = chunk->getCube_(pos.x, pos.y, pos.z);
    return rv;
  }
  return NULL;
}

glm::vec3
World::cameraToVoxelSpace(glm::vec3 cameraPosition)
{
  glm::vec3 halfAVoxel(0.5);
  glm::vec3 rv = (cameraPosition / glm::vec3(CUBE_SIZE)) + halfAVoxel;
  return rv;
}

Position
World::getLookedAtCube()
{
  Position rv;
  rv.valid = false;
  glm::vec3 voxelSpace = cameraToVoxelSpace(camera->position);

  int x = (int)floor(voxelSpace.x);
  int y = (int)floor(voxelSpace.y);
  int z = (int)floor(voxelSpace.z);

  int stepX = (camera->front.x > 0) ? 1 : -1;
  int stepY = (camera->front.y > 0) ? 1 : -1;
  int stepZ = (camera->front.z > 0) ? 1 : -1;

  // index<> already represents boundary if step<> is negative
  // otherwise add 1
  float tilNextX = x + ((stepX == 1) ? 1 : 0) -
                   (voxelSpace.x); // voxelSpace, because float position
  float tilNextY = y + ((stepY == 1) ? 1 : 0) - (voxelSpace.y);
  float tilNextZ = z + ((stepZ == 1) ? 1 : 0) - (voxelSpace.z);
  // what happens if x is negative though...

  float tMaxX = camera->front.x != 0 ? tilNextX / camera->front.x
                                     : std::numeric_limits<float>::infinity();

  float tMaxY = camera->front.y != 0 ? tilNextY / camera->front.y
                                     : std::numeric_limits<float>::infinity();

  float tMaxZ = camera->front.z != 0 ? tilNextZ / camera->front.z
                                     : std::numeric_limits<float>::infinity();

  float tDeltaX = camera->front.x != 0 ? 1 / abs(camera->front.x)
                                       : std::numeric_limits<float>::infinity();

  float tDeltaY = camera->front.y != 0 ? 1 / abs(camera->front.y)
                                       : std::numeric_limits<float>::infinity();

  float tDeltaZ = camera->front.z != 0 ? 1 / abs(camera->front.z)
                                       : std::numeric_limits<float>::infinity();

  int delta = 1;
  int limit = 20;

  glm::vec3 normal;
  glm::vec3 normalX = glm::vec3(stepX * -1, 0, 0);
  glm::vec3 normalY = glm::vec3(0, stepY * -1, 0);
  glm::vec3 normalZ = glm::vec3(0, 0, stepZ * -1);
  do {
    if (tMaxX < tMaxY) {
      if (tMaxX < tMaxZ) {
        tMaxX = tMaxX + tDeltaX;
        x = x + stepX;
        normal = normalX;
      } else {
        tMaxZ = tMaxZ + tDeltaZ;
        z = z + stepZ;
        normal = normalZ;
      }
    } else {
      if (tMaxY < tMaxZ) {
        tMaxY = tMaxY + tDeltaY;
        y = y + stepY;
        normal = normalY;
      } else {
        tMaxZ = tMaxZ + tDeltaZ;
        z = z + stepZ;
        normal = normalZ;
      }
    }
    auto closest = getCube(x, y, z);
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

ChunkMesh
World::meshSelectedCube(Position position)
{
  if (chunks.size() > 0 && chunks[0].size() > 0) {
    WorldPosition worldPosition =
      translateToWorldPosition(position.x, position.y, position.z);
    shared_ptr<Chunk> chunk =
      getChunk(worldPosition.chunkX, worldPosition.chunkZ);
    Position posInChunk{
      worldPosition.x, worldPosition.y, worldPosition.z, true, position.normal
    };
    return chunk->meshedFaceFromPosition(posInChunk);
  }
  return ChunkMesh{};
}

shared_ptr<DynamicObject>
World::getLookedAtDynamicObject()
{
  return dynamicObjects->getLookedAtObject(camera->position, camera->front);
}

void
World::cubeAction(Action toTake)
{
  Position lookingAt = getLookedAtCube();
  if (lookingAt.valid) {
    auto lookedAt = getCube(lookingAt.x, lookingAt.y, lookingAt.z);
    if (toTake == PLACE_CUBE) {
      int x = lookingAt.x + (int)lookingAt.normal.x;
      int y = lookingAt.y + (int)lookingAt.normal.y;
      int z = lookingAt.z + (int)lookingAt.normal.z;
      addCube(x, y, z, lookedAt->blockType());
      mesh();
    }
    if (toTake == REMOVE_CUBE) {
      WorldPosition pos =
        translateToWorldPosition(lookingAt.x, lookingAt.y, lookingAt.z);
      removeCube(pos);
      mesh();
    }
    if (toTake == SELECT_CUBE) {
      lookedAt->toggleSelect();
    }

    if (toTake == LOG_BLOCK_TYPE) {
      auto pos =
        translateToWorldPosition(lookingAt.x, lookingAt.y, lookingAt.z);
      auto chunk = getChunk(pos.chunkX, pos.chunkZ);
      auto cube = chunk->getCube_(pos.x, pos.y, pos.z);
      if (cube != NULL) {
        stringstream ss;
        ss << "lookedAtBlockType:" << cube->blockType() << ", (" << lookingAt.x
           << "," << lookingAt.z << ")";
        logger->critical(ss.str());
      }
    }
  }
}

void
World::dynamicObjectAction(Action toTake)
{
  if (toTake == OPEN_SELECTION_CODE) {
    logger->debug("edit code");
    auto view = registry->view<BoundingSphere, Scriptable>();
    for (auto [entity, boundingSphere, _scriptable] : view.each()) {
      stringstream debug;
      debug << "pos:" << camera->position.x << ", " << camera->position.y
            << ", " << camera->position.z;
      logger->debug(debug.str());
      if (systems::intersect(
            boundingSphere, camera->position, camera->front, 10.0)) {
        logger->debug("true");
        systems::editScript(registry, entity);
      }
    }
  }
}

void
World::action(Action toTake)
{
  cubeAction(toTake);
  dynamicObjectAction(toTake);
}

vector<Line>
World::getLines()
{
  return lines;
}

void
World::save(string filename)
{
  std::ofstream outputFile(filename);
  for (int chunkX = 0; chunkX < chunks.size(); chunkX++) {
    for (int chunkZ = 0; chunkZ < chunks[0].size(); chunkZ++) {
      shared_ptr<Chunk> chunk = chunks[chunkX][chunkZ];
      auto position = chunk->getPosition();
      auto size = chunk->getSize();
      for (int x = 0; x < size[0]; x++) {
        for (int y = 0; y < size[1]; y++) {
          for (int z = 0; z < size[2]; z++) {
            auto cube = chunk->getCube_(x, y, z);
            if (cube != NULL) {
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

void
World::load(string filename)
{
  std::ifstream inputFile(filename);
  char comma;
  float x, y, z;
  int blockType;
  while (inputFile >> x >> comma >> y >> comma >> z >> comma >> blockType) {
    addCube(x, y, z, blockType);
  }
  inputFile.close();
  mesh();
}

unsigned int
getIndexIntoRegion(int x, int y, int z)
{
  return (y * 16 + z) * 16 + x;
}

void
World::loadRegion(Coordinate regionCoordinate)
{
  auto region = loader->getRegion(regionCoordinate);
  for (auto chunk : region) {
    for (auto cube : chunk.cubePositions) {
      addCube(cube.x, cube.y, cube.z, cube.blockType);
    }
  }
}

void
World::initLoader(string folderName,
                  shared_ptr<blocks::TexturePack> texturePack)
{
  loader = new Loader(folderName, texturePack);
}

void
World::loadMinecraft()
{
  loadRegion(Coordinate{ 0, 0 });
  loadRegion(Coordinate{ -1, 0 });
  loadRegion(Coordinate{ -1, -1 });
  loadRegion(Coordinate{ 0, -1 });
  mesh();
}

void
World::loadLatest()
{
  std::filesystem::path dirPath("saves");

  if (!std::filesystem::exists(dirPath) ||
      !std::filesystem::is_directory(dirPath)) {
    throw "Directory saves/ doesn't exist or is not a directory";
  }

  std::string latestSave;

  for (const auto& entry : std::filesystem::directory_iterator(dirPath)) {
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

void
World::tick()
{
  ZoneScoped;
  systems::applyRotation(registry);
  systems::applyTranslations(registry);
  systems::updateAll(registry, renderer);
  if (dynamicObjects->damaged()) {
    renderer->updateDynamicObjects(dynamicObjects);
  }
  dynamicCube->move(glm::vec3(0.0f, 0.0f, 0.01f));
}
