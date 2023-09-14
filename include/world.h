#ifndef __WORLD_H__
#define __WORLD_H__

#include <vector>
#include <glm/glm.hpp>
#include "renderer.h"

class Renderer;
class World {
  std::vector<glm::vec3> cubes;
  Renderer* renderer;
public:
  const std::vector<glm::vec3>& getCubes();
  void addCube(glm::vec3);
  void attachRenderer(Renderer* renderer);
  World();
};

#endif
