#include "dynamicObject.h"
#include <algorithm>
#include <mutex>
#include <shared_mutex>

std::atomic<int> DynamicObject::nextId(0);

int
DynamicObject::id()
{
  return _id;
}

DynamicCube::DynamicCube(glm::vec3 position, glm::vec3 size, glm::vec3 color)
  : _position(position)
  , size(size)
  , color(color) {};

Renderable
DynamicCube::makeRenderable()
{
  setDamaged(false);
  Renderable renderable;

  // Half dimensions
  float hx = size.x * 0.5f;
  float hy = size.y * 0.5f;
  float hz = size.z * 0.5f;

  // Vertex positions relative to the center
  glm::vec3 vertices[] = {
    { hx, hy, hz },    { -hx, hy, hz },
    { -hx, -hy, hz },  { hx, -hy, hz }, // Front face
    { hx, hy, -hz },   { -hx, hy, -hz },
    { -hx, -hy, -hz }, { hx, -hy, -hz }, // Back face
    { hx, hy, hz },    { hx, -hy, hz },
    { hx, -hy, -hz },  { hx, hy, -hz }, // Right face
    { -hx, hy, hz },   { -hx, -hy, hz },
    { -hx, -hy, -hz }, { -hx, hy, -hz }, // Left face
    { hx, hy, hz },    { hx, hy, -hz },
    { -hx, hy, -hz },  { -hx, hy, hz }, // Top face
    { hx, -hy, hz },   { hx, -hy, -hz },
    { -hx, -hy, -hz }, { -hx, -hy, hz } // Bottom face
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
    renderable.colors.push_back(color);
  }

  return renderable;
}

glm::vec3
DynamicCube::getPosition()
{
  shared_lock<shared_mutex> lock(readWriteMutex);
  return _position;
}

void
DynamicCube::move(glm::vec3 addition)
{
  //unique_lock<shared_mutex> lock(readWriteMutex);
  setDamaged(true);
  setPosition(getPosition() + addition);
}

void
DynamicCube::setPosition(glm::vec3 newPos)
{
  unique_lock<shared_mutex> lock(readWriteMutex);
  _position = newPos;
}

bool
DynamicCube::damaged()
{
  shared_lock<shared_mutex> lock(readWriteMutex);
  return _damaged;
}

void
DynamicCube::setDamaged(bool damaged)
{
  unique_lock<shared_mutex> lock(readWriteMutex);
  _damaged = damaged;
}


Renderable
DynamicObjectSpace::makeRenderable()
{
  return renderableCache();
}

const Renderable&
DynamicObjectSpace::renderableCache()
{
  unique_lock<shared_mutex> lock(readWriteMutex);

  if (cacheInitialized && !needsFullRebuild) {
    return cachedRenderable;
  }

  cachedRenderable.vertices.clear();
  cachedRenderable.colors.clear();
  for (const auto& obj : objects) {
    if (obj == NULL || queuedRemovalIds.contains(obj->id())) {
      continue;
    }
    Renderable objRenderable = obj->makeRenderable();
    cachedRenderable.vertices.insert(cachedRenderable.vertices.end(),
                                     objRenderable.vertices.begin(),
                                     objRenderable.vertices.end());
    cachedRenderable.colors.insert(cachedRenderable.colors.end(),
                                   objRenderable.colors.begin(),
                                   objRenderable.colors.end());
  }

  pendingUpload.startVertex = 0;
  pendingUpload.vertexCount = cachedRenderable.vertices.size();
  pendingUpload.fullRefresh = true;
  cacheInitialized = true;
  needsFullRebuild = false;
  _damaged = false;
  return cachedRenderable;
}

RenderUploadSpan
DynamicObjectSpace::pendingUploadSpan() const
{
  shared_lock<shared_mutex> lock(readWriteMutex);
  return pendingUpload;
}

void
DynamicObjectSpace::clearPendingUpload()
{
  unique_lock<shared_mutex> lock(readWriteMutex);
  pendingUpload.vertexCount = 0;
  pendingUpload.fullRefresh = false;
  _damaged = false;
}

void
DynamicObjectSpace::addObject(shared_ptr<DynamicObject> obj)
{
  unique_lock<shared_mutex> lock(readWriteMutex);
  objects.push_back(obj);
  if (cacheInitialized && !needsFullRebuild && queuedRemovalIds.empty()) {
    Renderable objRenderable = obj->makeRenderable();
    const size_t appendStart = cachedRenderable.vertices.size();
    cachedRenderable.vertices.insert(cachedRenderable.vertices.end(),
                                     objRenderable.vertices.begin(),
                                     objRenderable.vertices.end());
    cachedRenderable.colors.insert(cachedRenderable.colors.end(),
                                   objRenderable.colors.begin(),
                                   objRenderable.colors.end());

    if (pendingUpload.vertexCount == 0) {
      pendingUpload.startVertex = appendStart;
      pendingUpload.vertexCount = objRenderable.vertices.size();
      pendingUpload.fullRefresh = false;
    } else if (!pendingUpload.fullRefresh &&
               pendingUpload.startVertex + pendingUpload.vertexCount ==
                 appendStart) {
      pendingUpload.vertexCount += objRenderable.vertices.size();
    } else {
      pendingUpload.startVertex = 0;
      pendingUpload.vertexCount = cachedRenderable.vertices.size();
      pendingUpload.fullRefresh = true;
    }
  } else {
    needsFullRebuild = true;
  }
  _damaged = true;
}

void
DynamicObjectSpace::queueRemoveObjectById(int id)
{
  queueRemoveObjectsById(vector<int>{ id });
}

void
DynamicObjectSpace::queueRemoveObjectsById(const vector<int>& ids)
{
  if (ids.empty()) {
    return;
  }

  unique_lock<shared_mutex> lock(readWriteMutex);
  for (int id : ids) {
    queuedRemovalIds.insert(id);
  }
  needsFullRebuild = true;
  _damaged = true;
}

void
DynamicObjectSpace::queueRemoveObjectsInBox(glm::vec3 min, glm::vec3 max)
{
  const glm::vec3 lower(std::min(min.x, max.x),
                        std::min(min.y, max.y),
                        std::min(min.z, max.z));
  const glm::vec3 upper(std::max(min.x, max.x),
                        std::max(min.y, max.y),
                        std::max(min.z, max.z));

  unique_lock<shared_mutex> lock(readWriteMutex);
  for (const auto& obj : objects) {
    if (obj == NULL) {
      continue;
    }
    const auto pos = obj->getPosition();
    if (pos.x < lower.x || pos.x > upper.x || pos.y < lower.y ||
        pos.y > upper.y || pos.z < lower.z || pos.z > upper.z) {
      continue;
    }
    queuedRemovalIds.insert(obj->id());
  }
  needsFullRebuild = true;
  _damaged = true;
}

void
DynamicObjectSpace::queueRemoveAllObjects()
{
  unique_lock<shared_mutex> lock(readWriteMutex);
  for (const auto& obj : objects) {
    if (obj != NULL) {
      queuedRemovalIds.insert(obj->id());
    }
  }
  needsFullRebuild = true;
  _damaged = true;
}

size_t
DynamicObjectSpace::flushQueuedRemovals()
{
  unique_lock<shared_mutex> lock(readWriteMutex);
  if (queuedRemovalIds.empty()) {
    return 0;
  }

  const auto before = objects.size();
  objects.erase(std::remove_if(objects.begin(),
                               objects.end(),
                               [&](const shared_ptr<DynamicObject>& obj) {
                                 return obj == NULL ||
                                        queuedRemovalIds.contains(obj->id());
                               }),
                objects.end());
  const auto removed = before - objects.size();
  queuedRemovalIds.clear();
  if (removed > 0) {
    cacheInitialized = false;
    needsFullRebuild = true;
    _damaged = true;
  }
  return removed;
}

bool
DynamicObjectSpace::damaged()
{
  shared_lock<shared_mutex> lock(readWriteMutex);
  return _damaged || !queuedRemovalIds.empty() || needsFullRebuild ||
         pendingUpload.vertexCount > 0;
}

shared_ptr<DynamicObject>
DynamicObjectSpace::getObjectById(int id)
{
  shared_lock<shared_mutex> lock(readWriteMutex);
  auto result = find_if(
    objects.begin(),
    objects.end(),
    [&](shared_ptr<DynamicObject> obj) -> bool {
      return obj != NULL && !queuedRemovalIds.contains(obj->id()) &&
             obj->id() == id;
    });
  if (result != objects.end()) {
    return *result;
  }
  return NULL;
}

vector<int>
DynamicObjectSpace::getObjectIds()
{
  vector<int> rv;
  shared_lock<shared_mutex> lock(readWriteMutex);
  for (auto object : objects) {
    if (object != NULL && !queuedRemovalIds.contains(object->id())) {
      rv.push_back(object->id());
    }
  }
  return rv;
}

shared_ptr<DynamicObject>
DynamicObjectSpace::getLookedAtObject(glm::vec3 position, glm::vec3 direction)
{
  return NULL;
}
