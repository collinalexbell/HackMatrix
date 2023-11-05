#include "chunk.h"

Chunk::Chunk() {
  data = make_unique<Cube* []>(xSize * ySize * zSize);
  for(int i=0; i < xSize*ySize*zSize; i++) {
    data[i] = NULL;
  }
}
Cube *Chunk::getCube(int x, int y, int z) {
  Cube *rv = data[index(x, y, z)];
  if(rv == NULL) {
    return &null;
  }
  return rv;
}
void Chunk::removeCube(int x, int y, int z) {
  delete data[index(x, y, z)];
  data[index(x, y, z)] = NULL;
}
void Chunk::addCube(Cube c, int x, int y, int z) {
  data[index(x, y, z)] = new Cube(c);
}

int Chunk::index(int x, int y, int z) { return x * ySize * zSize + y * zSize + z; }
