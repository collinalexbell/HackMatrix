#pragma once
#include <atomic>
#include <glm/glm.hpp>
#include <vector>
#include <memory>

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
  int id();
};

class DynamicObjectSpace: public DynamicObject {
  vector<shared_ptr<DynamicObject>> objects;
  bool _damaged = true;
public:
  void addObject(shared_ptr<DynamicObject> obj);
  Renderable makeRenderable() override;
  bool damaged() override;
  shared_ptr<DynamicObject> getObjectById(int id);
};

class DynamicCube: public DynamicObject {
  glm::vec3 position;
  glm::vec3 size;
  bool _damaged = true;
 public:
   DynamicCube(glm::vec3 position, glm::vec3 size);
   Renderable makeRenderable() override;
   void move(glm::vec3 addition);
   bool damaged() override;
};
