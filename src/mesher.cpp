#include "mesher.h"

Mesh Mesher::mesh(Chunk* chunk) {
  for (int dimension = 0; dimension < 3; ++dimension) {
    int i, j, k, l, w, h;
    int u = (dimension + 1) % 3;
    int v = (dimension + 2) % 3;
    int *x = new int[3];
    int *q = new int[3];

    vector<int> chunkSizes = chunk->getSize();

    bool *mask = new bool[chunkSizes[0]*chunkSizes[1]*chunkSizes[2]];
  }


  return Mesh{};
}
