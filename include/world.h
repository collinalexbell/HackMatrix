#ifndef __WORLD_H__
#define __WORLD_H__

#include <vector>
#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>
#include "renderer.h"
#include <unordered_map>

class Renderer;
class World {
  std::unordered_map<glm::vec3, int> cubes;
  std::unordered_map<glm::vec3, int> appCubes;
  Renderer* renderer;
public:
  const std::vector<glm::vec3> getCubes();
  const std::vector<glm::vec3> getAppCubes();
  void addCube(glm::vec3);
  void addAppCube(glm::vec3);
  void attachRenderer(Renderer* renderer);
  World();
};

#endif
