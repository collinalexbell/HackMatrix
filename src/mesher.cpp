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

    q[dimension] = 1;

    // Check each slice of the chunk one at a time
    for (x[dimension] = -1; x[dimension] < chunkSizes[dimension];) {
      // Compute the mask
      int n = 0;
      for (x[v] = 0; x[v] < chunkSizes[dimension]; ++x[v]) {
        for (x[u] = 0; x[u] < chunkSizes[dimension]; ++x[u]) {
          // q determines the direction (X, Y or Z) that we are searching
          // m.IsBlockAt(x,y,z) takes global map positions and returns true
          // if a block exists there

          bool blockCurrent =
            0 <= x[dimension] ? chunk->getCube(x[0], x[1], x[2]) != NULL
                        : true;
          bool blockCompare =
              x[dimension] < chunkSizes[dimension] - 1
                  ? chunk->getCube(x[0] + q[0], x[1] + q[1], x[2] + q[2]) != NULL
                  : true;

          // The mask is set to true if there is a visible face between two
          // blocks,
          //   i.e. both aren't empty and both aren't blocks
          mask[n++] = blockCurrent != blockCompare;
        }
      }
    }
  }
  return Mesh{};
}
