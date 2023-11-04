#include "chunk.h"

Chunk::Chunk() {
  for(int i=0; i < 128*128*128; i++) {
    data[i] = NULL;
  }
}
Cube *Chunk::getCube(int x, int y, int z) {
  Cube *rv = data[index(x,y,z)];
  if(rv == NULL) {
    return &null;
  }
  return rv;
}
void Chunk::removeCube(int x, int y, int z) {
  delete data[index(x,y,z)];
  data[index(x, y, z)] = NULL;
}
void Chunk::addCube(Cube c, int x, int y, int z) {
  data[index(x,y,z)] = new Cube(c);
}

int Chunk::index(int x, int y, int z) { return x * 128 * 128 + y * 128 + z; }
