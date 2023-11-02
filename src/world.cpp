#include "world.h"
#include "app.h"
#include "glm/geometric.hpp"
#include "renderer.h"
#include <algorithm>
#include <sstream>
#include <fstream>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtx/intersect.hpp>
#include <octree/octree.h>
#include <iostream>
#include <limits>
#include <cmath>
#include <filesystem>

using namespace std;



World::World(Camera *camera, bool debug) : camera(camera) {
  int max = CHUNK_SIZE - 1;
  logger = make_shared<spdlog::logger>("World", fileSink);
  logger->set_level(spdlog::level::debug);
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
}
World::~World() {}


const std::vector<Cube*> World::getCubes() {
  return vCubes;
}

const std::vector<Cube> World::getCubes(int _x1, int _y1, int _z1,
                                        int _x2, int _y2, int _z2) {
  int x1 = _x1 < _x2 ? _x1 : _x2;
  int x2 = _x1 < _x2 ? _x2 : _x1;
  int y1 = _y1 < _y2 ? _y1 : _y2;
  int y2 = _y1 < _y2 ? _y2 : _y1;
  int z1 = _z1 < _z2 ? _z1 : _z2;
  int z2 = _z1 < _z2 ? _z2 : _z1;

  vector<Cube> rv;
  for (int x = x1; x < x2; x++) {
    for (int y = y1; y < y2; y++) {
      for (int z = z1; z < z2; z++) {
        Cube cube = cubes.at(x, y, z);
        if (cube.blockType() != -1) {
          rv.push_back(cubes(x,y,z));
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
  stringstream ss;
  ss << "adding cube:" << x << "," << y << "," << z;
  logger->critical(ss.str());
  glm::vec3 pos(x, y, z);
  Cube cube(pos, blockType);
  addCube(cube);
}

void World::addCube(Cube cube) {
  int x = (int)cube.position().x;
  int y = (int)cube.position().y;
  int z = (int)cube.position().z;
  int orderIndex;
  const Cube *existing = &cubes.at(x,y,z);
  if(existing->blockType() >= 0) {
    removeCube(x,y,z, existing);
  }


  if(cube.blockType() >= 0) {
    int orderIndex = vCubes.size();
    updateDamage(orderIndex);
    cubes(x,y,z) = cube;
    vCubes.push_back(&cubes(x,y,z));

    if (renderer != NULL) {
      renderer->renderCube(orderIndex, cube);
    }
  }
}

void World::addLine(Line line) {
  if(line.color.r >= 0) {
    int i = lines.size();
    lines.push_back(line);
    stringstream ss;
    ss << "adding line (" << i << ")"
       << line.points[0].x << ","
       << line.points[0].y << ","
       << line.points[0].z << ","
       << line.points[1].x << ","
       << line.points[1].y << ","
       << line.points[1].z << ","
       << line.color.r;
    logger->critical(ss.str());
    logger->flush();
    if(renderer != NULL) {
      renderer->addLine(i, line);
    }
  } else {
    removeLine(line);
  }
}

void World::refreshRendererCubes() {
  vector<Cube*> allCubes = getCubes();
  if(isDamaged) {
    isDamaged = false;
    for (int i = damageIndex; i < allCubes.size(); i++) {
      renderer->renderCube(i, *allCubes[i]);
    }
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

  vector<glm::vec3> appCubesV = getAppCubes();
  for(int i=0; i < appCubesV.size(); i++){
    renderer->addAppCube(i, appCubesV[i]);
  }
}

void World::updateDamage(int index) {
  bool greaterDamage = !isDamaged || index < damageIndex;
  if (greaterDamage) {
    isDamaged = true;
    damageIndex = index;
  }
}

void World::removeCube(int x, int y, int z, const Cube* c) {
  auto it = find(vCubes.begin(), vCubes.end(), c);
    int index = it - vCubes.begin();
    updateDamage(index);
    vCubes.erase(it);
    cubes.erase(x, y, z);
}

void World::removeLine(Line l) {
  float EPSILON = 0.001;
  for(auto it = lines.begin(); it != lines.end(); it++) {
    glm::vec3 a0 = it->points[0];
    glm::vec3 b0 = it->points[1];
    glm::vec3 a1 = l.points[0];
    glm::vec3 b1 = l.points[1];
    if(glm::distance(a0,a1)<EPSILON && glm::distance(b0,b1)<EPSILON) {
      lines.erase(it);
    }
  }
}

void World::addApp(glm::vec3 pos, X11App* app) {
  int index = appCubes.size();
  appCubes.insert(std::pair<glm::vec3, int>(pos, index));
  apps.push_back(app);
  if(renderer != NULL) {
    logger->info("registerApp()");
    renderer->registerApp(app, index);
    logger->info("addAppCube");
    renderer->addAppCube(index, pos);

    stringstream debugInfo;
    debugInfo << "index:" << index << ", pos:" << pos.x << "," << pos.y << "," << pos.z;
    logger->debug(debugInfo.str());
    logger->flush();
  }
}

void World::removeApp(X11App *app) {
  int index = -1;
  for(int i=0; i<apps.size();i++) {
    if(apps[i] == app) {
      index = i;
    }
  }

  if(index < 0) {
    return;
  }

  auto it =
      std::find_if(appCubes.begin(), appCubes.end(),
                   [index](const std::pair<glm::vec3, int> &element) {
                     return element.second == index;
                   });

  appCubes.erase(it);
  apps.erase(apps.begin() + index);
  for(auto appKV = appCubes.begin(); appKV != appCubes.end(); appKV++) {
    if(appKV->second > index){
      appKV->second--;
    }
  }
  renderer->deregisterApp(index);
  refreshRendererCubes();
}

void World::attachRenderer(Renderer* renderer){
  this->renderer = renderer;
  refreshRendererCubes();
}

Cube* World::getCube(float x, float y, float z) {
  Cube* rv = &cubes(x,y,z);
  if(rv->blockType() != -1) {
    return rv;
  }
  return NULL;
}

int World::size() {
  return vCubes.size();
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
      addCube(x,y,z, lookedAt->blockType());
    }
    if(toTake == REMOVE_CUBE) {
      removeCube(lookingAt.x,lookingAt.y,lookingAt.z, lookedAt);
      refreshRendererCubes();
    }
    if(toTake == SELECT_CUBE) {
      lookedAt->selected() = lookedAt->selected() ? 0 : 1;
      refreshRendererCubes();
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

void World::save(string filename) {
  std::ofstream outputFile(filename);
  auto cubes = getCubes();
  for(auto it = cubes.begin(); it != cubes.end(); it++) {
    outputFile << (*it)->position().x << "," << (*it)->position().y << "," << (*it)->position().z << "," << (*it)->blockType() << endl;
  }
  outputFile.close();
}

void World::load(string filename) {
  std::ifstream inputFile(filename);
  char comma;
  float x, y, z;
  int blockType;
  while (inputFile >> x >> comma >> y >> comma >> z >> comma >> blockType) {
    stringstream ss;
    ss << "adding cube:" << x << "," << y << "," << z;
    logger->critical(ss.str());
    Cube c(glm::vec3(x, y, z), blockType);
    ss.clear();
    ss << "adding cube:" << c.position().x << "," << c.position().y << "," << c.position().z;
    logger->critical(ss.str());
    addCube(c);
  }
  inputFile.close();
}

void World::loadLatest() {
  std::filesystem::path dirPath("saves");

  if (!std::filesystem::exists(dirPath) ||
      !std::filesystem::is_directory(dirPath)) {
    throw "Directory saves/ doesn't exist or is not a directory";
  }

  std::string latestSave;

  for (const auto &entry : std::filesystem::directory_iterator(dirPath)) {
    if (entry.is_regular_file()) {
      std::string filename = entry.path().filename().string();

      // Check if the file has a ".save" extension
      if (filename.size() >= 5 &&
          filename.substr(filename.size() - 5) == ".save") {
        if (filename > latestSave) {
          latestSave = filename;
        }
      }
    }
  }

  load("saves/" + latestSave);
}
