#ifndef __WORLD_H__
#define __WORLD_H__

#include <vector>
#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>
#include "camera.h"
#include <unordered_map>
#include <octree/octree.h>

class Renderer;

struct Position {
  int x;
  int y;
  int z;
  bool valid;
  glm::vec3 normal;
};

struct Cube {
  glm::vec3 position;
  int blockType;
  bool operator==(const Cube& cmp);
};

enum Action {
  PLACE_CUBE,
  REMOVE_CUBE
};

class World {
  Renderer *renderer;
  Camera *camera;

  int CHUNK_SIZE = 128;
  int cubeCount = 0;
  Octree<Cube> cubes = Octree<Cube>(128, Cube{glm::vec3(0,0,0), -1});
  std::unordered_map<glm::vec3, int> appCubes;

  glm::vec3 cameraToVoxelSpace(glm::vec3 cameraPosition);
  Cube *getCube(float x, float y, float z);
  const std::vector<Cube> getCubes();

public:
  World(Camera *camera);
  ~World();
  void attachRenderer(Renderer *renderer);

  Position getLookedAtCube();
  const std::vector<glm::vec3> getAppCubes();

  void addCube(int x, int y, int z, int blockType);
  void removeCube(int x, int y, int z);
  void addAppCube(glm::vec3);
  int size();

  void action(Action);
};

#endif
