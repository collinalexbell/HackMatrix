#include "world.h"
#include <vector>
#include <glm/glm.hpp>



World::World(){}
World::~World() {}

const std::vector<Cube> World::getCubes() {
  std::vector<Cube> rv(cubes.size());
  for(auto kv: cubes) {
    Cube cube = kv.second;
    rv[cube.order] = cube;
  }
  return rv;
}
const std::vector<glm::vec3> World::getAppCubes() {
  std::vector<glm::vec3> appCubeKeys(appCubes.size());
  for(auto kv: appCubes) {
    appCubeKeys[kv.second] = kv.first;
  }
  return appCubeKeys;
}

void World::addCube(glm::vec3 pos, int blockType) {
  int index = cubes.size();
  Cube cube{pos, blockType, index};
  cubes.insert(std::pair<glm::vec3, Cube>(pos, cube));
  if(renderer != NULL) {
    renderer->addCube(index);
  }
}

void World::addAppCube(glm::vec3 cube) {
  int index = appCubes.size();
  appCubes.insert(std::pair<glm::vec3, int>(cube, index));
  if(renderer != NULL) {
    renderer->addAppCube(index);
  }
}

void World::attachRenderer(Renderer* renderer){
  this->renderer = renderer;
}
