#include "utility.h"
#include "chunk.h"

WorldPosition
translateToWorldPosition(int x, int y, int z)
{
  WorldPosition rv;
  auto chunkSize = Chunk::getSize();

  // x-------------
  if (x < 0) {
    int chunksXNegative = (x - chunkSize[0] + 1) / chunkSize[0];
    rv.chunkX = chunksXNegative;
    rv.x = x - (chunksXNegative * chunkSize[0]);
  } else {
    rv.x = x % chunkSize[0];
    rv.chunkX = x / chunkSize[0];
  }

  // y-------------
  rv.y = y;

  // z-------------
  if (z < 0) {
    int chunksZNegative = (z - chunkSize[2] + 1) / chunkSize[2];
    rv.chunkZ = chunksZNegative;
    rv.z = z - (chunksZNegative * chunkSize[2]);
  } else {
    rv.z = z % chunkSize[2];
    rv.chunkZ = z / chunkSize[2];
  }

  return rv;
}
