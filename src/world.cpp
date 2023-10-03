#include "world.h"
#include "renderer.h"
#include <vector>
#include <glm/glm.hpp>
#include <octree/octree.h>
#include <iostream>
#include <limits>
#include <cmath>

using namespace std;



World::World(){}
World::~World() {}

const std::vector<Cube> World::getCubes() {
  std::vector<Cube> rv(cubes.size());
  for(int x=0; x<CHUNK_SIZE; x++) {
    for(int y=0; y<CHUNK_SIZE; y++) {
      for(int z=0; z<CHUNK_SIZE; z++) {
        Cube cube = cubes(x,y,z);
        if(cube.blockType != -1) {
          rv[cube.order] = cube;
        }
      }
    }
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

void World::addCube(int x, int y, int z, int blockType) {
  int orderIndex = cubeCount++;
  glm::vec3 pos(x, y, z);
  Cube cube{pos, blockType, orderIndex};
  cubes(x,y,z) = cube;
  if(renderer != NULL) {
    renderer->addCube(orderIndex, cube);
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
  Cube* rv = &cubes(x,y,z);
  if(rv->blockType != -1) {
    return rv;
  }
  return NULL;
}

int World::size() {
  return cubeCount;
}


glm::vec3 World::cameraToVoxelSpace(glm::vec3 cameraPosition) {
  glm::vec3 rv = cameraPosition / glm::vec3(0.1,0.1,0.1);
  return rv;
}


Point World::rayCast(Camera* camera) {
  Point rv;
  rv.valid = false;
  glm::vec3 voxelSpace = cameraToVoxelSpace(camera->position);

  int x = (int)floor(voxelSpace.x);
  int y = (int)floor(voxelSpace.y);
  int z = (int)floor(voxelSpace.z);


  int stepX = ( camera->front.x > 0) ? 1 : -1;
  int stepY = ( camera->front.y > 0) ? 1 : -1;
  int stepZ = ( camera->front.z > 0) ? 1 : -1;

  // index<> already represents boundary if step<> is negative
  // otherwise add 1
  float tilNextX = x + ((stepX == 1) ? 1 : 0) - voxelSpace.x; // voxelSpace, because float position
  float tilNextY = y + ((stepY == 1) ? 1 : 0) - voxelSpace.y;
  float tilNextZ = z + ((stepZ == 1) ? 1 : 0) - voxelSpace.z;
  // what happens if x is negative though...


  float tMaxX = camera->front.x != 0 ?
    tilNextX / camera->front.x :
    std::numeric_limits<float>::infinity();

  float tMaxY = camera->front.y != 0 ?
    tilNextY / camera->front.y :
    std::numeric_limits<float>::infinity();

  float tMaxZ = camera->front.z != 0 ?
    tilNextZ / camera->front.z :
    std::numeric_limits<float>::infinity();


  float tDeltaX = camera->front.x != 0 ?
    1 / abs(camera->front.x) :
    std::numeric_limits<float>::infinity();

  float tDeltaY = camera->front.y != 0 ?
    1 / abs(camera->front.y) :
    std::numeric_limits<float>::infinity();

  float tDeltaZ = camera->front.z != 0 ?
    1 / abs(camera->front.z) :
    std::numeric_limits<float>::infinity();


  int delta = 1;
  int limit = 20;

  do {
    if(tMaxX < tMaxY) {
      if(tMaxX < tMaxZ){
        tMaxX = tMaxX + tDeltaX;
        x = x + stepX;
      } else {
        tMaxZ = tMaxZ + tDeltaZ;
        z = z + stepZ;
      }
    } else {
      if(tMaxY < tMaxZ) {
        tMaxY = tMaxY + tDeltaY;
        y = y + stepY;
      } else {
        tMaxZ = tMaxZ + tDeltaZ;
        z = z + stepZ;
      }
    }
    // positive guard until chunking is done
    if(x >= 0 && y >= 0 && z >= 0) {
      Cube* closest = getVoxel(x,y,z);
      if(closest!=NULL) {
        cout << "got one!"
             << x
             << ","
             << y
             << ","
             << z
             << ","
             << endl;
        rv.x = x;
        rv.y = y;
        rv.z = z;
        rv.valid = true;
        return rv;
      }
    }
  } while(tMaxX < limit || tMaxY < limit || tMaxZ < limit);

  return rv;
}
