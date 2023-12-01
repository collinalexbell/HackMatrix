#ifndef __WORLD_H__
#define __WORLD_H__

#include "app.h"
#include "coreStructs.h"
#include "mesher.h"
#include "cube.h"
#include "chunk.h"
#include "camera.h"
#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>
#include <octree/octree.h>
#include "logger.h"
#include <unordered_map>
#include <vector>
#include <queue>

class Renderer;

struct Line {
  glm::vec3 points[2];
  glm::vec3 color;
};

struct App {
  X11App* app;
  glm::vec3 position;
};

enum Action {
  PLACE_CUBE,
  REMOVE_CUBE,
  SELECT_CUBE,
  OPEN_SELECTION_CODE
};

class World {
  Mesher *mesher;
  std::shared_ptr<spdlog::logger> logger;
  Renderer *renderer = NULL;
  Camera *camera = NULL;

  int gotItCount = 0;
  vector<Line> lines;
  std::unordered_map<glm::vec3, int> appCubes;
  std::vector<X11App*> apps;
  Chunk cubes;
  int damageIndex = -1;
  bool isDamaged = false;
  glm::vec3 cameraToVoxelSpace(glm::vec3 cameraPosition);
  Cube *getCube(float x, float y, float z);
  const std::vector<Cube*> getCubes();
  const std::vector<Cube*> getCubes(int x1, int y1, int z1, int x2, int y2, int z2);
  void updateDamage(int index);
  queue<glm::vec3> availableAppPositions;

public:
  const float CUBE_SIZE = 0.1;
  World(Camera *camera, bool debug = false);
  ~World();
  void attachRenderer(Renderer *renderer);
  float getViewDistanceForWindowSize(X11App *app);

  X11App *getLookedAtApp();
  Position getLookedAtCube();
  const std::vector<glm::vec3> getAppCubes();
  glm::vec3 getAppPosition(X11App* app);

  void addCube(int x, int y, int z, int blockType);
  void removeCube(int x, int y, int z);
  void addLine(Line line);
  void removeLine(Line line);
  void addApp(glm::vec3, X11App* app);
  void addApp(X11App* app);
  void removeApp(X11App* app);
  int getIndexOfApp(X11App* app);

  void refreshRendererCubes();
  void action(Action);
  glm::vec3 getAppPosition(int appIndex);
  vector<Line> getLines();

  void save(string filename);
  void load(string filename);
  void loadLatest();

  void mesh(bool greedy = false);
  ChunkMesh meshSelectedCube(Position position);
};

#endif
