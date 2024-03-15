#include "IndexPool.h"

IndexPool::IndexPool(int maxIndex): maxIndex(maxIndex) {
  // Initialize the available indices
  for (int i = 0; i <= maxIndex; ++i) {
    availableIndices.push_back(i);
  }
}

int IndexPool::acquireIndex() {
  if (availableIndices.empty()) {
    // No available indices
    return -1;
  }

  // Get the smallest available index
  int index = availableIndices.front();
  availableIndices.erase(availableIndices.begin());
  usedIndices.push_back(index);
  return index;
}

void IndexPool::relinquishIndex(int index) {
  // Check if the index is valid
  if (index < 0 || index > maxIndex) {
    return;
  }

  // Check if the index is currently in use
  auto it = std::find(usedIndices.begin(), usedIndices.end(), index);
  if (it == usedIndices.end()) {
    return;
  }

  // Remove the index from the used indices vector
  usedIndices.erase(it);

  // Add the index back to the available indices vector
  availableIndices.push_back(index);

  // Sort the available indices vector
  std::sort(availableIndices.begin(), availableIndices.end());
}
