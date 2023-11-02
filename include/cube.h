#pragma once

#include <glm/glm.hpp>

class Cube {
  glm::vec3 _position = glm::vec3(0,0,0);
  int _blockType;
  int _selected = 0;

public:
  Cube();
  Cube(glm::vec3 position, int blockType);
  Cube(glm::vec3 position, int blockType, int selected);
  bool operator==(const Cube &cmp);
  glm::vec3 position() const;
  glm::vec3 &position();
  int blockType() const;
  int &blockType();
  int selected() const;
  int &selected();
};
