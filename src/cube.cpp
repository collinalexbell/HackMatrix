#include "cube.h"
#include <limits>
#include <memory>
#include <sstream>


glm::vec3 Cube::zeroVec(0, 0, 0);
int Cube::zeroBlock(-1);
int Cube::zeroSelected(0);

bool Cube::operator==(const Cube &cmp) {
  bool xEq = this->position().x == cmp.position().x;
  bool yEq = this->position().x == cmp.position().y;
  bool zEq = this->position().z == cmp.position().z;
  bool blockTypeEq = this->blockType() == cmp.blockType();
  return xEq && yEq && zEq && blockTypeEq;
}

Cube::Cube(const Cube &cpy) {
  _blockType = cpy.blockType();
  _position = cpy.position();
  _selected = cpy.selected();
}

Cube &Cube::operator=(const Cube &other) {
  this->blockType() = other.blockType();
  this->selected() = other.selected();
  this->position() = other.position();
  return *this;
}

void Cube::init(glm::vec3 position, int blockType, int selected) {
  _position = position;
  _blockType = blockType;
  _selected = selected;
}

Cube::Cube() {
  init(zeroVec, zeroBlock, zeroSelected);
}

Cube::Cube(glm::vec3 position, int blockType) {
  init(position, blockType, 0);
}

Cube::Cube(glm::vec3 position, int blockType, int selected) {
  init(position, blockType, selected);
}
Cube::~Cube() {}

glm::vec3 Cube::position() const {
  return _position;
}

glm::vec3 &Cube::position() {
  return _position;
}

int Cube::blockType() const {
  return _blockType;
}

int &Cube::blockType() {
  return _blockType;
}

int Cube::selected() const {
  return _selected;
}

int &Cube::selected() {
  return _selected;
}

void Cube::toggleSelect() {
  selected() = selected() ? 0 : 1;
}



