#pragma once

#include "logger.h"

#include <memory>
#include <vector>
#include <map>
#include <glm/glm.hpp>
using namespace std;

struct CubeBuffer {
  glm::vec3 *vecs;
  int *ints;
  int damageIndex;
  int damageSize;
  int totalSize;
};

class Cube {
  static glm::vec3 zeroVec;
  static int zeroBlock;
  static int zeroSelected;
  static int damageIndex;
  static vector<glm::vec3> vecs;
  static vector<int> ints;
  static vector<shared_ptr<int>> indices;
  static bool isInit;

  static vector<int> toErase;
  static vector<int> referenceCount;
  static shared_ptr<spdlog::logger> logger;

  static void initClass();

  shared_ptr<int> index;

  void init(glm::vec3, int blockType, int selected);

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
  void toggleSelect();
  void remove();

  static CubeBuffer render();
  static int size() { return vecs.size();}
};
