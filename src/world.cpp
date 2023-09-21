#include "world.h"
#include <vector>
#include <glm/glm.hpp>

World::World(){}
World::World(int d){
  int r = d/2;
  for (int x = -r; x<=r; x++) {
    for (int y = -r; y<=r; y++) {
      for (int z = -d; z<0; z++) {
        cubes.push_back(glm::vec3((float)x, (float)y, (float)z));
      }
    }
  }
}

const std::vector<glm::vec3>& World::getCubes() {
  return cubes;
}
const std::vector<glm::vec3>& World::getAppCubes() {
  return appCubes;
}

void World::addCube(glm::vec3 cube) {
  int index = cubes.size();
  cubes.push_back(cube);
  if(renderer != NULL) {
    renderer->addCube(index);
  }
}

void World::addAppCube(glm::vec3 cube) {
  int index = appCubes.size();
  appCubes.push_back(cube);
  if(renderer != NULL) {
    renderer->addAppCube(index);
  }
}

void World::attachRenderer(Renderer* renderer){
  this->renderer = renderer;
}
