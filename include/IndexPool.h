#pragma once

#include <vector>
#include <algorithm>

class IndexPool {
public:
  IndexPool(int maxIndex);
  int acquireIndex();
  void relinquishIndex(int index);
private:
  int maxIndex;
  std::vector<int> availableIndices;
  std::vector<int> usedIndices;
};
