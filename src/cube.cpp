#include "cube.h"

bool Cube::operator==(const Cube &cmp) {
  bool xEq = this->position().x == cmp.position().x;
  bool yEq = this->position().x == cmp.position().y;
  bool zEq = this->position().z == cmp.position().z;
  bool blockTypeEq = this->blockType() == cmp.blockType();
  return xEq && yEq && zEq && blockTypeEq;
}

Cube::Cube(): Cube(glm::vec3(0,0,0), -1, 0) {}

Cube::Cube(glm::vec3 position, int blockType): Cube(position, blockType, 0) {}

Cube::Cube(glm::vec3 position, int blockType, int selected):
  _position(position), _blockType(blockType), _selected(selected) {}

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
