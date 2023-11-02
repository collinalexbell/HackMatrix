#pragma once

#include <vector>
#include <glm/glm.hpp>
using namespace std;

class Cube {
  static vector<glm::vec3> vecs;
  static vector<int> ints;
  int index;

public:
  Cube();
  Cube(const Cube &cpy);
  Cube(glm::vec3 position, int blockType);
  Cube(glm::vec3 position, int blockType, int selected);
  Cube &operator=(const Cube &other);
  bool operator==(const Cube &cmp);
  glm::vec3 position() const;
  glm::vec3 &position();
  int blockType() const;
  int &blockType();
  int selected() const;
  int &selected();
};
