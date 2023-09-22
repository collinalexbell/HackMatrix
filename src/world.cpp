#include "world.h"
#include <vector>
#include <glm/glm.hpp>

World::World(){}

const std::vector<glm::vec3> World::getCubes() {
  std::vector<glm::vec3> cubeKeys(cubes.size());
  for(auto kv: cubes) {
    cubeKeys[kv.second] = kv.first;
  }
  return cubeKeys;
}
const std::vector<glm::vec3> World::getAppCubes() {
  std::vector<glm::vec3> appCubeKeys(appCubes.size());
  for(auto kv: appCubes) {
    appCubeKeys[kv.second] = kv.first;
  }
  return appCubeKeys;
}

void World::addCube(glm::vec3 cube) {
  int index = cubes.size();
  cubes.insert(std::pair<glm::vec3, int>(cube, index));
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
