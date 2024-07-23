#pragma once
#include <glm/glm.hpp>

struct AbsolutePosition
{
  int x;
  int y;
  int z;
};

struct Position
{
  int x;
  int y;
  int z;
  bool valid;
  glm::vec3 normal;
};

struct WorldPosition
{
  int x;
  int y;
  int z;
  int chunkX;
  int chunkZ;
};
