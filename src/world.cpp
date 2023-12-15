#include "world.h"
#include "app.h"
#include "chunk.h"
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
namespace fs = std::filesystem;

World::World(Camera *camera, bool debug) : camera(camera) {
  initLogger();
  initAppPositions();
  initChunks();
}

void World::initLogger() {
  logger = make_shared<spdlog::logger>("World", fileSink);
  logger->set_level(spdlog::level::debug);
}

void World::initAppPositions() {
  float z = 10.0;
  availableAppPositions.push(glm::vec3(5.0, 1.0, z));
  availableAppPositions.push(glm::vec3(6.2, 1.0, z));
  availableAppPositions.push(glm::vec3(6.2, 1.75, z));
  availableAppPositions.push(glm::vec3(6.2, 2.50, z));
  availableAppPositions.push(glm::vec3(7.4, 1.0, z));
  availableAppPositions.push(glm::vec3(4.7, 1.75, z));
  availableAppPositions.push(glm::vec3(7.4, 1.75, z));
}

void World::initChunks() {
  for (int x = -10; x <= 10; x++) {
    chunks.push_back(deque<Chunk *>());
    for (int z = -10; z <= 10; z++) {
      chunks.back().push_back(new Chunk(x, 0, z));
    }
  }
  middleIndex = calculateMiddleIndex();
}

World::~World() {}

void World::mesh(bool realTime) {
  double currentTime = glfwGetTime();
  vector<ChunkMesh> m;
  int sizeX = chunks[0][0]->getSize()[0];
  int sizeZ = chunks[0][0]->getSize()[2];
  for(int x = 0; x < chunks.size(); x++) {
    for(int z = 0; z < chunks[x].size(); z++) {
        m.push_back(chunks[x][z]->mesh(realTime));
    }
  }
  stringstream ss;
  ss << "time: " << glfwGetTime() - currentTime;
  logger->debug(ss.str());
  logger->flush();
  renderer->updateChunkMeshBuffers(m);
}

const vector<Cube*> World::getCubes() {
  if(chunks.size()>0 && chunks[0].size() > 0) {
    auto chunkSize = chunks[0][0]->getSize();
    return getCubes(0,0,0,
                    chunkSize[0]*chunks.size(),
                    chunkSize[1],
                    chunkSize[2]*chunks[0].size());
  }
  return vector<Cube*>{};
}

const std::vector<Cube*> World::getCubes(int _x1, int _y1, int _z1,
                                        int _x2, int _y2, int _z2) {
  int x1 = _x1 < _x2 ? _x1 : _x2;
  int x2 = _x1 < _x2 ? _x2 : _x1;
  int y1 = _y1 < _y2 ? _y1 : _y2;
  int y2 = _y1 < _y2 ? _y2 : _y1;
  int z1 = _z1 < _z2 ? _z1 : _z2;
  int z2 = _z1 < _z2 ? _z2 : _z1;

  vector<Cube*> rv;
  for (int x = x1; x < x2; x++) {
    for (int y = y1; y < y2; y++) {
      for (int z = z1; z < z2; z++) {
        Cube *cube = getCube(x, y, z);
        if (cube->blockType() != -1) {
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

WorldPosition World::translateToWorldPosition(int x, int y, int z) {
  WorldPosition rv;
  auto chunkSize = chunks[0][0]->getSize();

  // x-------------
  if (x < 0) {
    int chunksXNegative = (x - chunkSize[0] + 1) / chunkSize[0];
    rv.chunkX = chunksXNegative;
    rv.x = x - (chunksXNegative * chunkSize[0]);
  }
  else {
    rv.x = x % chunkSize[0];
    rv.chunkX = x / chunkSize[0];
  }

  //y-------------
  rv.y = y;

  // z-------------
  if (z < 0) {
    int chunksZNegative = (z - chunkSize[2] + 1) / chunkSize[2];
    rv.chunkZ = chunksZNegative;
    rv.z = z - (chunksZNegative * chunkSize[2]);
  } else {
    rv.z = z % chunkSize[2];
    rv.chunkZ = z / chunkSize[2];
  }

  return rv;
}

ChunkIndex World::getChunkIndex(int x, int z) {
  ChunkPosition minChunkPosition = chunks[0][0]->getPosition();
  ChunkPosition maxChunkPosition = chunks.back().back()->getPosition();
  ChunkIndex index;
  index.x = x - minChunkPosition.x;
  index.z = z - minChunkPosition.z;
  if (index.x >= 0 && index.x < chunks.size() && index.z >= 0 &&
      index.z < chunks[0].size()) {
    index.isValid = true;
  } else {
    index.isValid = false;
  }

  return index;
}

void World::loadChunksIfNeccissary() {
  ChunkIndex curIndex = playersChunkIndex();
  if(curIndex.x < middleIndex.x) {
    deque<Chunk*> chunksToRm = chunks.back();
    for(Chunk* toRm: chunksToRm) {
      delete toRm;
    }
    chunks.pop_back();
    deque<Chunk *> toAdd;
    auto pos = chunks[0][0]->getPosition();
    auto size = chunks[0][0]->getSize();
    for(int i = 0; i < chunks[0].size(); i++) {
      toAdd.push_back(new Chunk(pos.x-1, pos.y, pos.z+i));
    }
    chunks.push_front(toAdd);
    int sign = std::signbit(pos.z) ? -1 : 1;
    for(int i = 0; i<size[0]; i++) {
      for(int j = 0; j<size[2]*chunks[0].size(); j++) {
        addCube((pos.x-1)*size[0]+i,5,pos.z*size[2]+j,3);
      }
    }
    mesh();
  }
  if(curIndex.x > middleIndex.x) {
    deque<Chunk *> chunksToRm = chunks.front();
    for (Chunk *toRm : chunksToRm) {
      delete toRm;
    }
    chunks.pop_front();
    deque<Chunk *> toAdd;
    auto pos = chunks.back()[0]->getPosition();
    auto size = chunks.back()[0]->getSize();
    for (int i = 0; i < chunks[0].size(); i++) {
      toAdd.push_back(new Chunk(pos.x + 1, pos.y, pos.z + i));
    }
    chunks.push_back(toAdd);
    int sign = std::signbit(pos.z) ? -1 : 1;
    for (int i = 0; i < size[0]; i++) {
      for (int j = 0; j < size[2] * chunks[0].size(); j++) {
        addCube(pos.x * size[0] + i, 5, pos.z * size[2] + j, 3);
      }
    }
    mesh();
  }
  if(curIndex.z < middleIndex.z) {
    // to be implemented later
  }
  if(curIndex.z > middleIndex.z) {
    // to be implemented later
  }
}

ChunkIndex World::calculateMiddleIndex() {
  ChunkIndex middleIndex;
  // even, make a choice, left or right (no middle)
  //20/2 = 10;
  // odd, index is clearly correct
  //21/2 = 10;

  middleIndex.x = chunks.size()/2;
  middleIndex.z = chunks[0].size()/2;
  middleIndex.isValid = true;
  return middleIndex;
}

ChunkIndex World::playersChunkIndex() {
  glm::vec3 voxelSpace = cameraToVoxelSpace(camera->position);
  auto worldPosition = translateToWorldPosition(voxelSpace.x, voxelSpace.y, voxelSpace.z);
  ChunkIndex rv = getChunkIndex(worldPosition.chunkX, worldPosition.chunkZ);
  return rv;
}


Chunk *World::getChunk(int x, int z) {
  ChunkIndex index = getChunkIndex(x, z);
  if(index.isValid) {
    return chunks[index.x][index.z];
  }
  return NULL;
}

void World::addCube(int x, int y, int z, int blockType) {
  WorldPosition pos = translateToWorldPosition(x, y, z);
  removeCube(pos);
  if (blockType >= 0) {
    glm::vec3 positionInChunk(pos.x, pos.y, pos.z);
    Cube cube(positionInChunk, blockType);
    Chunk *chunk = getChunk(pos.chunkX, pos.chunkZ);
    if (chunk != NULL) {
      chunk->addCube(cube, pos.x, pos.y, pos.z);
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

void World::removeCube(WorldPosition pos) {
  Chunk* chunk = getChunk(pos.chunkX, pos.chunkZ);
  if(chunk != NULL) {
    Cube *c = chunk->getCube_(pos.x,pos.y,pos.z);
    if(c != NULL) {
      chunk->removeCube(pos.x, pos.y, pos.z);
    }
  }
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

void World::addApp(X11App* app) {
  if(!app->isAccessory()) {
    glm::vec3 pos = availableAppPositions.front();
    addApp(pos, app);
    if(availableAppPositions.size() > 1) {
      availableAppPositions.pop();
    }
  } else {
    /*
    int index = appCubes.size();
    apps.push_back(app);
    renderer->registerApp(app, index);
    */
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
  if(chunks.size() > 0 && chunks[0].size() > 0) {
    WorldPosition pos = translateToWorldPosition(x,y,z);
    Chunk *chunk = getChunk(pos.chunkX, pos.chunkZ);
    Cube* rv = chunk->getCube_(pos.x, pos.y, pos.z);
    return rv;
  }
  return NULL;
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
    Cube *closest = getCube(x, y, z);
    if (closest != NULL) {
      rv.x = x;
      rv.y = y;
      rv.z = z;
      rv.normal = normal;
      rv.valid = true;
      return rv;
    }
  } while (tMaxX < limit || tMaxY < limit || tMaxZ < limit);

  return rv;
}

ChunkMesh World::meshSelectedCube(Position position) {
  if(chunks.size() > 0 && chunks[0].size() > 0) {
    WorldPosition worldPosition = translateToWorldPosition(position.x, position.y, position.z);
    Chunk *chunk = getChunk(worldPosition.chunkX, worldPosition.chunkZ);
    Position posInChunk{
      worldPosition.x, worldPosition.y, worldPosition.z,
      true, position.normal
    };
    return chunk->meshedFaceFromPosition(posInChunk);
  }
  return ChunkMesh{};
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
      mesh();
    }
    if(toTake == REMOVE_CUBE) {
      WorldPosition pos = translateToWorldPosition(lookingAt.x, lookingAt.y, lookingAt.z);
      removeCube(pos);
      mesh();
    }
    if(toTake == SELECT_CUBE) {
      lookedAt->toggleSelect();
    }
    if(toTake == OPEN_SELECTION_CODE) {
      logger->info("open_selection_code");
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
  for(int chunkX = 0; chunkX < chunks.size(); chunkX++){
    for(int chunkZ = 0; chunkZ < chunks[0].size(); chunkZ++) {
      Chunk* chunk = chunks[chunkX][chunkZ];
      auto position = chunk->getPosition();
      auto size = chunk->getSize();
      for(int x=0; x<size[0]; x++) {
        for(int y=0; y<size[1]; y++) {
          for(int z=0; z<size[2]; z++) {
            Cube* cube = chunk->getCube_(x,y,z);
            if(cube != NULL) {
            outputFile << cube->position().x + size[0] * position.x << ","
                      << cube->position().y << ","
                      << cube->position().z + size[2] * position.z << ","
                      << cube->blockType() << endl;
            }
          }
        }
      }
    }
  }
  outputFile.close();
}

void World::load(string filename) {
  std::ifstream inputFile(filename);
  char comma;
  float x, y, z;
  int blockType;
  while (inputFile >> x >> comma >> y >> comma >> z >> comma >> blockType) {
    addCube(x,y,z,blockType);
  }
  inputFile.close();
  mesh();
}

std::vector<std::string> getFilesInFolder(const std::string &folderPath) {
  std::vector<std::string> files;

  for (const auto &entry : fs::directory_iterator(folderPath)) {
    if (fs::is_regular_file(entry.path())) {
      files.push_back(entry.path().filename().string());
    }
  }

  return files;
}

std::array<int, 2> getCoordinatesFromRegionFilename(const std::string &filename) {
  std::array<int, 2> coordinates = {
      0, 0}; // Initialize coordinates with default values

  try {
    // Extracting X and Z coordinates from the filename
    size_t startPos = filename.find_first_of(".") + 1;
    size_t endPos = filename.find_last_of(".");

    std::string coordsSubstring = filename.substr(startPos, endPos - startPos);
    size_t period = coordsSubstring.find_first_of(".");

    coordinates[0] = std::stoi(coordsSubstring.substr(0, period));
    coordinates[1] = std::stoi(coordsSubstring.substr(period + 1));
  } catch (const std::exception &e) {
    std::cerr << "Exception occurred: " << e.what() << std::endl;
    // Handle any exceptions, or you can leave the coordinates as default (0, 0)
  }

  return coordinates;
}

void World::loadMinecraft(string folderName) {
  auto fileNames = getFilesInFolder(folderName);
  for(auto fileName: fileNames) {
    stringstream ss;
    auto coords = getCoordinatesFromRegionFilename(fileName);
    auto key = Coordinate(coords);
    regionFiles[key] = fileName;
  }
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

void World::tick(){
  //loadChunksIfNeccissary();
}
