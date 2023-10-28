#ifndef __WORLD_H__
#define __WORLD_H__

#include "app.h"
#include "camera.h"
#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>
#include <octree/octree.h>
#include "logger.h"
#include <unordered_map>
#include <vector>

class Renderer;

struct Position {
  int x;
  int y;
  int z;
  bool valid;
  glm::vec3 normal;
};

struct Line {
  glm::vec3 points[2];
  glm::vec3 color;
};

struct Cube {
  glm::vec3 position;
  int blockType;
  bool operator==(const Cube& cmp);
};

struct App {
  X11App* app;
  glm::vec3 position;
};

enum Action {
  PLACE_CUBE,
  REMOVE_CUBE
};

class World {
  std::shared_ptr<spdlog::logger> logger;
  Renderer *renderer = NULL;
  Camera *camera = NULL;

  int gotItCount = 0;
  int CHUNK_SIZE = 128;
  int cubeCount = 0;
  vector<Line> lines;
  std::unordered_map<glm::vec3, int> appCubes;
  std::vector<X11App*> apps;
  Octree<Cube> cubes = Octree<Cube>(128, Cube{glm::vec3(0, 0, 0), -1});
  glm::vec3 cameraToVoxelSpace(glm::vec3 cameraPosition);
  Cube *getCube(float x, float y, float z);
  const std::vector<Cube> getCubes();

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
  void addCube(Cube cube);
  void removeCube(int x, int y, int z);
  void addLine(Line line);
  void removeLine(Line line);
  void addApp(glm::vec3, X11App* app);
  void removeApp(X11App* app);
  int getIndexOfApp(X11App* app);
  int size();

  void refreshRendererCubes();
  void action(Action);
  glm::vec3 getAppPosition(int appIndex);
  vector<Line> getLines();

  void save(string filename);
  void load(string filename);
  void loadLatest();
};

#endif
