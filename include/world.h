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
  int order;
};
class World {
  int CHUNK_SIZE = 128;
  int cubeCount = 0;
  Octree<Cube> cubes = Octree<Cube>(128, Cube{glm::vec3(0,0,0), -1, -1});
  std::unordered_map<glm::vec3, int> appCubes;
  Renderer* renderer;
  glm::vec3 cameraToVoxelSpace(glm::vec3 cameraPosition);
  Camera* camera;
public:
  Position rayCast(Camera* camera);
  const std::vector<Cube> getCubes();
  const std::vector<glm::vec3> getAppCubes();
  void addCube(int x, int y, int z, int blockType);
  void addAppCube(glm::vec3);
  void attachRenderer(Renderer* renderer);
  Cube* getVoxel(float x, float y, float z);
  int size();
  void action();
  World(Camera* camera);
  ~World();
};

#endif
