#include "world.h"
#include "app.h"
#include "glm/geometric.hpp"
#include "renderer.h"
#include <sstream>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtx/intersect.hpp>
#include <octree/octree.h>
#include <iostream>
#include <limits>
#include <cmath>

using namespace std;



World::World(Camera *camera, bool debug) : camera(camera) {
  int max = CHUNK_SIZE - 1;
  if(debug) {
    addCube(0, 0, 0, 0);
    addCube(max, 0, 0, 0);
    addCube(0,max,0,0);
    addCube(0,0,max, 0);
    addCube(max, max, 0, 0);
    addCube(0, max, max,0);
    addCube(max,0, max, 0);
    addCube(max, 0, max, 0);
    addCube(max, max, max, 0);
  }

  Line line = {{glm::vec3(0, 0, 0), glm::vec3(max/10.0, max/10.0, max/10.0)},
               glm::vec3(0.1, 0.1, 0.1)};
  addLine(line);
  logger = make_shared<spdlog::logger>("World", fileSink);
}
World::~World() {}

bool Cube::operator==(const Cube &cmp){
  bool xEq = this->position.x == cmp.position.x;
  bool yEq = this->position.x == cmp.position.y;
  bool zEq = this->position.z == cmp.position.z;
  bool blockTypeEq = this->blockType == cmp.blockType;
  return xEq && yEq && zEq && blockTypeEq;
}

const std::vector<Cube> World::getCubes() {
  std::vector<Cube> rv;
  for(int x=0; x<CHUNK_SIZE; x++) {
    for(int y=0; y<CHUNK_SIZE; y++) {
      for(int z=0; z<CHUNK_SIZE; z++) {
        Cube cube = cubes.at(x,y,z);
        if(cube.blockType != -1) {
          rv.push_back(cube);
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
  glm::vec3 pos(x, y, z);
  Cube cube{pos, blockType};
  addCube(cube);
}

void World::addCube(Cube cube) {
  if(cube.blockType >= 0) {
    int orderIndex = cubeCount++;
    // I may want to think about replacing glm::vec3 with int position in cube
    cubes((int)cube.position.x,
          (int)cube.position.y,
          (int)cube.position.z) = cube;
    if (renderer != NULL) {
      renderer->addCube(orderIndex, cube);
    }
  } else {
    removeCube((int)cube.position.x,
               (int)cube.position.y,
               (int)cube.position.z);
  }
}

void World::addLine(Line line) {
  int i = lines.size();
  lines.push_back(line);
  if(renderer != NULL) {
    renderer->addLine(i, lines[i]);
  }
}

void World::refreshRenderer() {
  vector<Cube> allCubes = getCubes();
  cubeCount = allCubes.size();
  for (int i = 0; i < allCubes.size(); i++) {
    renderer->addCube(i, allCubes[i]);
  }
  for(int i = 0; i < lines.size(); i++) {
    stringstream lineInfo;
    Line l = lines[i];
    lineInfo << "adding line:"
             << l.points[0].x << ", "
             << l.points[0].y << ","
             << l.points[0].z << ","
             << l.points[1].x << ","
             << l.points[1].y << ","
             << l.points[1].z;
    logger->critical(lineInfo.str());
    renderer->addLine(i, lines[i]);
  }
}

void World::removeCube(int x, int y, int z) {
  cubes.erase(x,y,z);
}

void World::addApp(glm::vec3 pos, X11App* app) {
  int index = appCubes.size();
  appCubes.insert(std::pair<glm::vec3, int>(pos, index));
  apps.push_back(app);
  if(renderer != NULL) {
    renderer->registerApp(app, index);
    renderer->addAppCube(index, pos);
  }
}

void World::attachRenderer(Renderer* renderer){
  this->renderer = renderer;
  refreshRenderer();
}

Cube* World::getCube(float x, float y, float z) {
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
  glm::vec3 halfAVoxel(0.5);
  glm::vec3 rv = (cameraPosition / glm::vec3(CUBE_SIZE)) + halfAVoxel;
  return rv;
}

struct Intersection {
  glm::vec3 intersectionPoint;
  float dist;
};

Intersection intersectLineAndPlane(glm::vec3 linePos, glm::vec3 lineDir, glm::vec3 planePos) {
  Intersection intersection;
  glm::vec3 normLineDir = glm::normalize(lineDir);
  glm::intersectRayPlane(linePos, normLineDir, planePos, glm::vec3(0,0,1), intersection.dist);
  intersection.intersectionPoint = (normLineDir * intersection.dist) + linePos;
  return intersection;
}

X11App* World::getLookedAtApp(){
  float DIST_LIMIT = 1.5;
  float height = 0.74;
  float width = 1.0;
  for (glm::vec3 appPosition : getAppCubes()) {
    Intersection intersection = intersectLineAndPlane(camera->position, camera->front, appPosition);
    float minX = appPosition.x - (width / 3);
    float maxX = appPosition.x + (width / 3);
    float minY = appPosition.y - (height / 3);
    float maxY = appPosition.y + (height / 3);
    float x = intersection.intersectionPoint.x;
    float y = intersection.intersectionPoint.y;
    if(x>minX && x<maxX && y>minY && y<maxY && intersection.dist < DIST_LIMIT) {
      int index = appCubes.at(appPosition);
      X11App* app = apps[index];
      return app;
    }
  }
  return NULL;
}


Position World::getLookedAtCube() {
  Position rv;
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
  float tilNextX = x + ((stepX == 1) ? 1 : 0) - (voxelSpace.x); // voxelSpace, because float position
  float tilNextY = y + ((stepY == 1) ? 1 : 0) - (voxelSpace.y);
  float tilNextZ = z + ((stepZ == 1) ? 1 : 0) - (voxelSpace.z);
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

  glm::vec3 normal;
  glm::vec3 normalX = glm::vec3(stepX*-1, 0, 0);
  glm::vec3 normalY = glm::vec3(0, stepY*-1, 0);
  glm::vec3 normalZ = glm::vec3(0, 0, stepZ*-1);
  do {
    if(tMaxX < tMaxY) {
      if(tMaxX < tMaxZ){
        tMaxX = tMaxX + tDeltaX;
        x = x + stepX;
        normal = normalX;
      } else {
        tMaxZ = tMaxZ + tDeltaZ;
        z = z + stepZ;
        normal = normalZ;
      }
    } else {
      if(tMaxY < tMaxZ) {
        tMaxY = tMaxY + tDeltaY;
        y = y + stepY;
        normal = normalY;
      } else {
        tMaxZ = tMaxZ + tDeltaZ;
        z = z + stepZ;
        normal = normalZ;
      }
    }
    // positive guard until chunking is done
    if(x >= 0 && y >= 0 && z >= 0 && x < CHUNK_SIZE && y < CHUNK_SIZE && z < CHUNK_SIZE) {
      Cube* closest = getCube(x,y,z);
      if(closest!=NULL) {
        rv.x = x;
        rv.y = y;
        rv.z = z;
        rv.normal = normal;
        rv.valid = true;
        return rv;
      }
    }
  } while(tMaxX < limit || tMaxY < limit || tMaxZ < limit);

  return rv;
}

void World::action(Action toTake) {
  Position lookingAt = getLookedAtCube();
  if(lookingAt.valid) {
    Cube* lookedAt = getCube(lookingAt.x, lookingAt.y, lookingAt.z);
    if(toTake == PLACE_CUBE) {
      int x = lookingAt.x + (int)lookingAt.normal.x;
      int y = lookingAt.y + (int)lookingAt.normal.y;
      int z = lookingAt.z + (int)lookingAt.normal.z;
      addCube(x,y,z, lookedAt->blockType);
    }
    if(toTake == REMOVE_CUBE) {
      removeCube(lookingAt.x,lookingAt.y,lookingAt.z);
      refreshRenderer();
    }
  }
}

int World::getIndexOfApp(X11App *app) {
  for(int i = 0; i < apps.size(); i++) {
    if(app == apps[i]){
      return i;
    }
  }
  return -1;
}

float World::getViewDistanceForWindowSize(X11App *app) {
  // view = projection^-1 * gl_vertex * vertex^-1
  float glVertexX = float(app->width)/1920;
  glm::vec4 gl_pos = glm::vec4(10000,0,0,0);
  float zBest;
  float target = glVertexX;
    for (float z = 0.0; z <= 10.5; z = z + 0.001) {
      glm::vec4 candidate;
      candidate = renderer->projection * glm::vec4(0.5, 0, -z, 1);
      candidate = candidate/candidate.w;
      if(abs(candidate.x - target) < abs(gl_pos.x - target)) {
        gl_pos = candidate;
        zBest = z;
      }
    }
    return zBest;
}


glm::vec3 World::getAppPosition(X11App* app) {
  int index = -1;
  for(int i=0; i<apps.size(); i++) {
    if(app == apps[i]) {
      index = i;
    }
  }
  if(index == -1) {
    throw "app not found";
  }

  return getAppCubes()[index];

}

vector<Line> World::getLines() {
  return lines;
}
