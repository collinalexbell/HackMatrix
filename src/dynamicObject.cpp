#include "dynamicObject.h"
#include <algorithm>
#include <mutex>
#include <shared_mutex>

std::atomic<int> DynamicObject::nextId(0);

int DynamicObject::id() {
  return _id;
}

DynamicCube::DynamicCube(glm::vec3 position, glm::vec3 size)
  : _position(position), size(size) {};

Renderable DynamicCube::makeRenderable() {
  setDamaged(false);
  Renderable renderable;

  // Half dimensions
  float hx = size.x * 0.5f;
  float hy = size.y * 0.5f;
  float hz = size.z * 0.5f;

  // Vertex positions relative to the center
  glm::vec3 vertices[] = {
      {hx, hy, hz},    {-hx, hy, hz},
      {-hx, -hy, hz},  {hx, -hy, hz}, // Front face
      {hx, hy, -hz},   {-hx, hy, -hz},
      {-hx, -hy, -hz}, {hx, -hy, -hz}, // Back face
      {hx, hy, hz},    {hx, -hy, hz},
      {hx, -hy, -hz},  {hx, hy, -hz}, // Right face
      {-hx, hy, hz},   {-hx, -hy, hz},
      {-hx, -hy, -hz}, {-hx, hy, -hz}, // Left face
      {hx, hy, hz},    {hx, hy, -hz},
      {-hx, hy, -hz},  {-hx, hy, hz}, // Top face
      {hx, -hy, hz},   {hx, -hy, -hz},
      {-hx, -hy, -hz}, {-hx, -hy, hz} // Bottom face
  };

  // Triangles (two per face)
  int indices[] = {
      0,  1,  2,  0,  2,  3,  // Front
      4,  7,  6,  4,  6,  5,  // Back
      8,  11, 10, 8,  10, 9,  // Right
      12, 13, 14, 12, 14, 15, // Left
      16, 19, 18, 16, 18, 17, // Top
      20, 21, 22, 20, 22, 23  // Bottom
  };

  auto position = getPosition();

  // Fill vertices for each triangle
  for (int i = 0; i < 36; i++) {
    renderable.vertices.push_back(position + vertices[indices[i]]);
  }

  return renderable;
}

glm::vec3 DynamicCube::getPosition() {
  shared_lock<shared_mutex> lock(readWriteMutex);
  return _position;
}

void DynamicCube::move(glm::vec3 addition) {
  setDamaged(true);
  setPosition(getPosition() + addition);
}

void DynamicCube::setPosition(glm::vec3 newPos) {
  unique_lock<shared_mutex> lock(readWriteMutex);
  _position = newPos;
}

bool DynamicCube::damaged(){
  shared_lock<shared_mutex> lock(readWriteMutex);
  return _damaged;
}

void DynamicCube::setDamaged(bool damaged) {
  unique_lock<shared_mutex> lock(readWriteMutex);
  _damaged = damaged;
}


Renderable DynamicObjectSpace::makeRenderable() {
  _damaged = false;
  Renderable combinedRenderable;
  shared_lock<shared_mutex> lock(readWriteMutex);
  // Iterate over all dynamic objects and combine their renderables
  for (const auto &obj : objects) {
    Renderable objRenderable = obj->makeRenderable();
    combinedRenderable.vertices.insert(combinedRenderable.vertices.end(),
                                       objRenderable.vertices.begin(),
                                       objRenderable.vertices.end());
  }

  return combinedRenderable;
}

void DynamicObjectSpace::addObject(shared_ptr<DynamicObject> obj) {
  unique_lock<shared_mutex> lock(readWriteMutex);
  objects.push_back(obj);
}

bool DynamicObjectSpace::damaged() {
  bool rv = _damaged;
  shared_lock<shared_mutex> lock(readWriteMutex);
  for(auto element: objects) {
    rv |= element->damaged();
  }
  return rv;
}

shared_ptr<DynamicObject> DynamicObjectSpace::getObjectById(int id) {
  shared_lock<shared_mutex> lock(readWriteMutex);
  auto result = find_if(objects.begin(), objects.end(), [id](shared_ptr<DynamicObject> obj) -> bool {
    return obj->id() == id;
  });
  if(result != objects.end()) {
    return *result;
  }
  return NULL;
}

vector<int> DynamicObjectSpace::getObjectIds() {
  vector<int> rv;
  shared_lock<shared_mutex> lock(readWriteMutex);
  for(auto object: objects) {
    rv.push_back(object->id());
  }
  return rv;
}
