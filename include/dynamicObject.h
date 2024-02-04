#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <memory>

using namespace std;

struct Renderable {
  vector<glm::vec3> vertices;
};

class DynamicObject {
 public:
  virtual Renderable makeRenderable() = 0;
};

class DynamicObjectSpace: public DynamicObject {
  vector<shared_ptr<DynamicObject>> objects;
  bool _damaged = true;
 public:
   void addObject(shared_ptr<DynamicObject> obj);
   Renderable makeRenderable() override;
   bool damaged();
};

class DynamicCube: public DynamicObject {
  glm::vec3 position;
  glm::vec3 size;
 public:
   DynamicCube(glm::vec3 position, glm::vec3 size);
   Renderable makeRenderable() override;
};
