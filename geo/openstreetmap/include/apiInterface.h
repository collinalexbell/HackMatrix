
#pragma once

class ApiInterface {
 public:
  virtual void addCube(int x, int y, int z, int blockType) = 0;
};
