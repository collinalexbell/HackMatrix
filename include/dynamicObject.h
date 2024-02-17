#pragma once
#include <atomic>
#include <glm/glm.hpp>
#include <memory>
#include <mutex>
#include <vector>
#include <shared_mutex>

using namespace std;

struct Renderable {
  vector<glm::vec3> vertices;
};

class DynamicObject {
  int _id;
  static std::atomic_int nextId;
public:
  DynamicObject() : _id(nextId.fetch_add(1)) {}
  virtual Renderable makeRenderable() = 0;
  virtual bool damaged() = 0;
  virtual void move(glm::vec3) = 0;
  virtual glm::vec3 getPosition() {return glm::vec3(0,0,0);};
  int id();
};

class DynamicObjectSpace: public DynamicObject {
  vector<shared_ptr<DynamicObject>> objects;
  bool _damaged = true;
  shared_mutex readWriteMutex;
public:
  void addObject(shared_ptr<DynamicObject> obj);
  Renderable makeRenderable() override;
  bool damaged() override;
  void move(glm::vec3) override {
    throw "DynamicObjectSpace.move() unimplemented";
  }
  shared_ptr<DynamicObject> getObjectById(int id);
  vector<int> getObjectIds();
};

class DynamicCube: public DynamicObject {
  glm::vec3 _position;
  glm::vec3 size;
  atomic_bool _damaged = true;
  void setPosition(glm::vec3 newPos);
  shared_mutex readWriteMutex;
public:
  DynamicCube(glm::vec3 position, glm::vec3 size);
  Renderable makeRenderable() override;
  void move(glm::vec3 addition) override;
  bool damaged() override;
  void setDamaged(bool);
  glm::vec3 getPosition() override;
};
