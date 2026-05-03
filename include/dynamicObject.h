#pragma once
#include <atomic>
#include <glm/glm.hpp>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <unordered_set>
#include <vector>

using namespace std;

struct Renderable
{
  vector<glm::vec3> vertices;
  vector<glm::vec3> colors;
};

struct RenderUploadSpan
{
  size_t startVertex = 0;
  size_t vertexCount = 0;
  bool fullRefresh = true;
};

class DynamicObject
{
  int _id;
  static std::atomic_int nextId;

public:
  DynamicObject()
    : _id(nextId.fetch_add(1))
  {
  }
  virtual Renderable makeRenderable() = 0;
  virtual bool damaged() = 0;
  virtual void move(glm::vec3) = 0;
  virtual glm::vec3 getPosition() { return glm::vec3(0, 0, 0); };
  int id();
};

struct DynamicObjectIntersection
{
  float distance;
  shared_ptr<DynamicObject> object;
  glm::vec3 point;
};

class DynamicObjectSpace : public DynamicObject
{
  vector<shared_ptr<DynamicObject>> objects;
  unordered_set<int> queuedRemovalIds;
  Renderable cachedRenderable;
  RenderUploadSpan pendingUpload;
  bool cacheInitialized = false;
  bool needsFullRebuild = true;
  bool _damaged = true;
  mutable shared_mutex readWriteMutex;

public:
  void addObject(shared_ptr<DynamicObject> obj);
  void queueRemoveObjectById(int id);
  void queueRemoveObjectsById(const vector<int>& ids);
  void queueRemoveObjectsInBox(glm::vec3 min, glm::vec3 max);
  void queueRemoveAllObjects();
  size_t flushQueuedRemovals();
  Renderable makeRenderable() override;
  const Renderable& renderableCache();
  RenderUploadSpan pendingUploadSpan() const;
  void clearPendingUpload();
  bool damaged() override;
  void move(glm::vec3) override
  {
    throw "DynamicObjectSpace.move() unimplemented";
  }
  shared_ptr<DynamicObject> getObjectById(int id);
  vector<int> getObjectIds();
  shared_ptr<DynamicObject> getLookedAtObject(glm::vec3 position,
                                              glm::vec3 direction);
  vector<DynamicObjectIntersection> findIntersections(glm::vec3 position,
                                                      glm::vec3 direction);
};

class DynamicCube : public DynamicObject
{
  glm::vec3 _position;
  glm::vec3 size;
  glm::vec3 color;
  atomic_bool _damaged = true;
  void setPosition(glm::vec3 newPos);
  shared_mutex readWriteMutex;

public:
  DynamicCube(glm::vec3 position, glm::vec3 size, glm::vec3 color);
  Renderable makeRenderable() override;
  void move(glm::vec3 addition) override;
  bool damaged() override;
  void setDamaged(bool);
  glm::vec3 getPosition() override;
};
