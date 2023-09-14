#ifndef __WORLD_H__
#define __WORLD_H__

#include <vector>
#include <glm/glm.hpp>

class World {
  std::vector<glm::vec3> cubes;
public:
  const std::vector<glm::vec3>& getCubes();
  World();
};

#endif
