#include "cube.h"

vector<glm::vec3> Cube::vecs;
vector<int> Cube::ints;

bool Cube::operator==(const Cube &cmp) {
  bool xEq = this->position().x == cmp.position().x;
  bool yEq = this->position().x == cmp.position().y;
  bool zEq = this->position().z == cmp.position().z;
  bool blockTypeEq = this->blockType() == cmp.blockType();
  return xEq && yEq && zEq && blockTypeEq;
}

Cube::Cube(const Cube &cpy) {
  index = cpy.index;
}

Cube &Cube::operator=(const Cube &other) {
  if (this != &other) {
    this->index = other.index;
  }
  return *this;
}

Cube::Cube(){
  index = vecs.size();
  vecs.push_back(glm::vec3(0, 0, 0));
  ints.push_back(-1);
  ints.push_back(0);
}

Cube::Cube(glm::vec3 position, int blockType) {
  index = vecs.size();
  vecs.push_back(position);
  ints.push_back(blockType);
  ints.push_back(0);
}

Cube::Cube(glm::vec3 position, int blockType, int selected) {
  index = vecs.size();
  vecs.push_back(position);
  ints.push_back(blockType);
  ints.push_back(selected);
}

glm::vec3 Cube::position() const {
  return vecs[index];
}

glm::vec3 &Cube::position() {
  return vecs[index];
}

int Cube::blockType() const {
  return ints[index*2];
}

int &Cube::blockType() {
  return ints[index*2];
}

int Cube::selected() const {
  return ints[index*2+1];
}

int &Cube::selected() {
  return ints[index*2+1];
}
