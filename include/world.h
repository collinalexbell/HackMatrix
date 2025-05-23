#ifndef __WORLD_H__
#define __WORLD_H__

#include "app.h"
#include "blocks.h"
#include "coreStructs.h"
#include "cube.h"
#include "chunk.h"
#include "camera.h"
#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>
#include <memory>
#include <mutex>
#include "entity.h"
#include "logger.h"
#include <spdlog/common.h>
#include <unordered_map>
#include <vector>
#include <queue>
#include <future>
#include <optional>
#include "loader.h"
#include "dynamicObject.h"
#include "worldInterface.h"
#include "model.h"

class Renderer;

struct App
{
  X11App* app;
  glm::vec3 position;
};

class World : public WorldInterface
{
  shared_ptr<spdlog::logger> logger;
  shared_ptr<DynamicCube> dynamicCube;
  shared_ptr<EntityRegistry> registry;
  Renderer* renderer = NULL;
  Camera* camera = NULL;
  vector<Line> lines;
  deque<deque<shared_ptr<Chunk>>> chunks;
  map<DIRECTION, deque<future<deque<shared_ptr<Chunk>>>>> preloadedChunks;
  int WORLD_SIZE = 9;
  int PRELOAD_SIZE = 3;
  int damageIndex = -1;
  mutex preloadMutex;
  bool isDamaged = false;
  glm::vec3 cameraToVoxelSpace(glm::vec3 cameraPosition);
  shared_ptr<Cube> getCube(float x, float y, float z);
  const vector<shared_ptr<Cube>> getCubes();
  const vector<shared_ptr<Cube>> getCubes(int x1,
                                          int y1,
                                          int z1,
                                          int x2,
                                          int y2,
                                          int z2);
  void updateDamage(int index);
  void removeCube(WorldPosition position);
  ChunkIndex getChunkIndex(int x, int z);
  ChunkIndex playersChunkIndex();
  ChunkIndex calculateMiddleIndex();
  array<ChunkPosition, 2> getNextPreloadedChunkPositions(DIRECTION direction,
                                                         int nextPreloadCount,
                                                         bool isInitial);
  OrthoginalPreload orthoginalPreload(DIRECTION direction, preload::SIDE side);
  void loadNextPreloadedChunkDeque(DIRECTION direction, bool initial = false);
  void transferChunksToPreload(DIRECTION movementDirection,
                               deque<shared_ptr<Chunk>> slice);
  ChunkIndex middleIndex;
  void initChunks();
  void initLogger(spdlog::sink_ptr loggerSink);
  void loadChunksIfNeccissary();
  void initPreloadedChunks();
  void logCoordinates(array<Coordinate, 2> c, string label);
  shared_ptr<DynamicObjectSpace> dynamicObjects;
  void cubeAction(Action toTake);
  void dynamicObjectAction(Action toTake);

public:
  void tick() override;
  const float CUBE_SIZE = 0.1;
  World(shared_ptr<EntityRegistry>,
        Camera* camera,
        shared_ptr<blocks::TexturePack> texturePack,
        bool debug = false,
        spdlog::sink_ptr loggerSink = fileSink);
  ~World();
  void attachRenderer(Renderer* renderer) override;
  Loader* loader;
  Position getLookedAtCube() override;

  void addCube(int x, int y, int z, int blockType) override;
  void addLine(Line line) override;
  void removeLine(Line line) override;

  void action(Action) override;
  vector<Line> getLines() override;

  void save(string filename) override;
  void load(string filename) override;
  void loadRegion(Coordinate regionCoordinate) override;
  void loadMinecraft() override;
  void initLoader(string folderName,
                  shared_ptr<blocks::TexturePack> texturePack) override;
  void loadLatest() override;
  shared_ptr<DynamicObject> getLookedAtDynamicObject();
  void mesh(bool realTime = true) override;
  ChunkMesh meshSelectedCube(Position position) override;
  shared_ptr<Chunk> getChunk(int chunkX, int chunkZ) override;
  shared_ptr<DynamicObjectSpace> getDynamicObjects() override
  {
    return dynamicObjects;
  };
};

#endif
