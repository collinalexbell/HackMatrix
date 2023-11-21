#include "chunk.h"

Chunk::Chunk() {
  data = make_unique<Cube* []>(size[0] * size[1] * size[2]);
  for(int i=0; i < size[0]*size[1]*size[2]; i++) {
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

Cube *Chunk::getCube_(int x, int y, int z) {
  return data[index(x, y, z)];
}

void Chunk::removeCube(int x, int y, int z) {
  delete data[index(x, y, z)];
  data[index(x, y, z)] = NULL;
}
void Chunk::addCube(Cube c, int x, int y, int z) {
  data[index(x, y, z)] = new Cube(c);
}

ChunkMesh Chunk::mesh() {
  ChunkMesh rv;
  int totalSize = size[0] * size[1] * size[2];
  for(int i = 0; i<totalSize; i++) {
    if(data[i] != NULL) {
    }
  }
  return rv;
}

int Chunk::index(int x, int y, int z) { return x * size[1] * size[2] + y * size[2] + z; }

const vector<int> Chunk::getSize() {
  return size;
}
