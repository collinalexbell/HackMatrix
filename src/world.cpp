#include "world.h"
#include <vector>
#include <glm/glm.hpp>

World::World() {
  for (int x = -25; x<25; x++) {
    for (int y = -25; y<25; y++) {
      for (int z = -50; z<0; z++) {
        cubes.push_back(glm::vec3((float)x, (float)y, (float)z));
      }
    }
  }
}

const std::vector<glm::vec3>& World::getCubes() {
  return cubes;
}
