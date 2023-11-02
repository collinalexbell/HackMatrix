#pragma once

#include <vector>
#include <map>
#include <glm/glm.hpp>
using namespace std;

struct CubeBuffer {
  glm::vec3 *vecs;
  int *ints;
  int damageIndex;
  int damageSize;
};

class Cube {
  static vector<glm::vec3> vecs;
  static vector<int> ints;
  static vector<int*> indices;

  static vector<int> toErase;
  static vector<int> referenceCount;

  int index;

public:
  Cube();
  Cube(const Cube &cpy);
  Cube(glm::vec3 position, int blockType);
  Cube(glm::vec3 position, int blockType, int selected);
  ~Cube();
  Cube &operator=(const Cube &other);
  bool operator==(const Cube &cmp);
  glm::vec3 position() const;
  glm::vec3 &position();
  int blockType() const;
  int &blockType();
  int selected() const;
  int &selected();

  static CubeBuffer render();
};
