#pragma once

#include "logger.h"

#include <memory>
#include <vector>
#include <map>
#include <glm/glm.hpp>
using namespace std;

struct CubeBuffer
{
  glm::vec3* vecs;
  int* ints;
  int damageIndex;
  int damageSize;
  int totalSize;
};

class Cube
{
  static glm::vec3 zeroVec;
  static int zeroBlock;
  static int zeroSelected;
  glm::vec3 _position;
  int _blockType;
  int _selected;

  static void initClass();

  void init(glm::vec3, int blockType, int selected);

public:
  Cube();
  Cube(const Cube& cpy);
  Cube(glm::vec3 position, int blockType);
  Cube(glm::vec3 position, int blockType, int selected);
  ~Cube();
  Cube& operator=(const Cube& other);
  bool operator==(const Cube& cmp);

  // TODO: remove position, uneeded ATM;
  glm::vec3 position() const;
  glm::vec3& position();

  int blockType() const;
  int& blockType();
  int selected() const;
  int& selected();
  void toggleSelect();
};
