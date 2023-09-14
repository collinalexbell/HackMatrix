#include "world.h"
#include <vector>
#include <glm/glm.hpp>

World::World() {
  for (int x = -5; x<=5; x++) {
    for (int y = -5; y<=5; y++) {
      for (int z = -5; z<0; z++) {
        cubes.push_back(glm::vec3((float)x, (float)y, (float)z));
      }
    }
  }
}

const std::vector<glm::vec3>& World::getCubes() {
  return cubes;
}

void World::addCube(glm::vec3 cube) {
  int index = cubes.size();
  cubes.push_back(cube);
  if(renderer != NULL) {
    renderer->addCube(index);
  }
}

void World::attachRenderer(Renderer* renderer){
  this->renderer = renderer;
}
