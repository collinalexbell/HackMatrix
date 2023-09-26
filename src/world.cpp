#include "world.h"
#include "renderer.h"
#include <vector>
#include <glm/glm.hpp>
#include <iostream>

using namespace std;



World::World(){
  Cube nullCube;
  nullCube.blockType = -1;
  cubes = std::vector<Cube>(CHUNK_SIZE*CHUNK_SIZE*CHUNK_SIZE, nullCube);
}
World::~World() {}

const std::vector<Cube> World::getCubes() {
  cout << "getting cubes" << endl;
  std::vector<Cube> rv(cubeCount);
  for(auto cube: cubes) {
    if(cube.blockType != -1) {
      rv[cube.order] = cube;
    }
  }
  cout << "returning cubes" << endl;
  return rv;
}
const std::vector<glm::vec3> World::getAppCubes() {
  std::vector<glm::vec3> appCubeKeys(appCubes.size());
  for(auto kv: appCubes) {
    appCubeKeys[kv.second] = kv.first;
  }
  return appCubeKeys;
}

void World::addCube(int x, int y, int z, int blockType) {
  int index = x * (CHUNK_SIZE*CHUNK_SIZE) + y * (CHUNK_SIZE) + z;
  int orderIndex = cubeCount++;
  glm::vec3 pos(x, y, z);
  Cube cube{pos, blockType, orderIndex};
  cubes[index] = cube;
  if(renderer != NULL) {
    renderer->addCube(orderIndex, cubes[index]);
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

Cube* World::getVoxel(float x, float y, float z) {
  int index = x * (CHUNK_SIZE*CHUNK_SIZE) + y * (CHUNK_SIZE) + z;
  Cube* rv = &cubes[index];
  if(rv->blockType != -1) {
    return rv;
  }
  return NULL;
}

int World::size() {
  return cubeCount;
}
