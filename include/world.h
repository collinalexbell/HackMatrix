#ifndef __WORLD_H__
#define __WORLD_H__

#include <vector>
#include <glm/glm.hpp>
#include "renderer.h"

class Renderer;
class World {
  std::vector<glm::vec3> cubes;
  std::vector<glm::vec3> appCubes;
  Renderer* renderer;
public:
  const std::vector<glm::vec3>& getCubes();
  const std::vector<glm::vec3>& getAppCubes();
  void addCube(glm::vec3);
  void addAppCube(glm::vec3);
  void attachRenderer(Renderer* renderer);
  World();
  World(int d);
};

#endif
