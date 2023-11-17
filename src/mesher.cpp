#include "mesher.h"

Mesh Mesher::mesh(Chunk* chunk) {
  int i, j, k, l, w, h, u, v;
  int x[3];
  int q[3];
  for (int dimension = 0; dimension < 3; ++dimension) {
    u = (dimension + 1) % 3;
    v = (dimension + 2) % 3;
    x[0] = 0;
    x[1] = 0;
    x[2] = 0;
    q[0] = 0;
    q[1] = 0;
    q[2] = 0;
    q[dimension] = 1;

    vector<int> chunkSizes = chunk->getSize();

    bool *mask = new bool[chunkSizes[0]*chunkSizes[1]*chunkSizes[2]];

    q[dimension] = 1;

    // Check each slice of the chunk one at a time
    for (x[dimension] = -1; x[dimension] < chunkSizes[dimension];) {
      // Compute the mask
      int n = 0;
      for (x[v] = 0; x[v] < chunkSizes[v]; ++x[v]) {
        for (x[u] = 0; x[u] < chunkSizes[u]; ++x[u]) {
          bool blockCurrent =
            0 <= x[dimension] ? chunk->getCube(x[0], x[1], x[2]) != NULL
                        : true;
          bool blockCompare =
              x[dimension] < chunkSizes[dimension] - 1
                  ? chunk->getCube(x[0] + q[0], x[1] + q[1], x[2] + q[2]) != NULL
                  : true;

          mask[n++] = blockCurrent != blockCompare;
        }
      }
    }
  }
  return Mesh{};
}
