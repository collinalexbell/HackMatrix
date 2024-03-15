#pragma once

#include "app.h"
#include "blocks.h"
#include "camera.h"
#include "chunk.h"
#include "coreStructs.h"
#include "cube.h"
#include "dynamicObject.h"
#include "loader.h"
#include "logger.h"
#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>
#include <memory>
#include <mutex>
#include <octree/octree.h>
#include <optional>
#include <queue>
#include <unordered_map>
#include <vector>

class Renderer;

struct Line {
  glm::vec3 points[2];
  glm::vec3 color;
};

enum Action {
  PLACE_CUBE,
  REMOVE_CUBE,
  SELECT_CUBE,
  OPEN_SELECTION_CODE,
  LOG_BLOCK_TYPE
};

class WorldInterface {
public:
  virtual void tick() = 0;
  virtual void attachRenderer(Renderer *renderer) = 0;
  virtual Position getLookedAtCube() = 0;

  virtual void addCube(int x, int y, int z, int blockType) = 0;
  virtual void addLine(Line line) = 0;
  virtual void removeLine(Line line) = 0;

  virtual void action(Action) = 0;
  virtual vector<Line> getLines() = 0;

  virtual void save(string filename) = 0;
  virtual void load(string filename) = 0;
  virtual void loadRegion(Coordinate regionCoordinate) = 0;
  virtual void loadMinecraft() = 0;
  virtual void initLoader(string folderName,
                  shared_ptr<blocks::TexturePack> texturePack) = 0;
  virtual void loadLatest() = 0;

  virtual void mesh(bool realTime = true) = 0;
  virtual ChunkMesh meshSelectedCube(Position position) = 0;
  virtual shared_ptr<Chunk> getChunk(int chunkX, int chunkZ) = 0;
  virtual shared_ptr<DynamicObjectSpace> getDynamicObjects() = 0;
};
