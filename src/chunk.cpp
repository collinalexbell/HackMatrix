#include "chunk.h"
#include <cassert>
#include <memory>

Chunk::Chunk(int x, int y, int z): posX(x), posY(y), posZ(z) {
  data = make_unique<Cube* []>(size[0] * size[1] * size[2]);
  for(int i=0; i < size[0]*size[1]*size[2]; i++) {
    data[i] = NULL;
  }
}

Chunk::Chunk() {
  mesher = make_unique<Mesher>();
  posX = 0; posY=0; posZ=0;
  data = make_unique<Cube *[]>(size[0] * size[1] * size[2]);
  for (int i = 0; i < size[0] * size[1] * size[2]; i++) {
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
  if(x>=0 && x < size[0] && y >=0 && y < size[1] &&  z >= 0 && z < size[2]) {
    return data[index(x, y, z)];
  }
  return NULL;
}

void Chunk::removeCube(int x, int y, int z) {
  setDamaged();
  delete data[index(x, y, z)];
  data[index(x, y, z)] = NULL;
}
void Chunk::addCube(Cube c, int x, int y, int z) {
  setDamaged();
  data[index(x, y, z)] = new Cube(c);
}

void Chunk::setDamaged() {
  damagedSimple = true;
  damagedGreedy = true;
}

ChunkMesh Chunk::meshedFaceFromPosition(Position position) {
  return mesher->meshedFaceFromPosition(this, position);
}

ChunkMesh Chunk::mesh(bool realTime) {
  if(!realTime || !damagedGreedy) {
    if(damagedGreedy) {
      cachedGreedyMesh = mesher->meshGreedy(posX, posZ, this);
      damagedGreedy = false;
    }
    return cachedGreedyMesh;
  } else {
    if(damagedSimple) {
      cachedSimpleMesh = mesher->simpleMesh(posX, posZ, this);
      damagedSimple = false;
    }
    return cachedSimpleMesh;
  }
}

int Chunk::index(int x, int y, int z) {
  return x * size[1] * size[2] + y * size[2] + z;
}

ChunkCoords Chunk::getCoords(int index) {
  ChunkCoords rv;
  rv.z = index % size[2];
  index /= size[2];

  rv.y = index % size[1];
  index /= size[1];

  rv.x = index;
  return rv;
}

const vector<int> Chunk::getSize() {
  return size;
}

ChunkPosition Chunk::getPosition() {
  return ChunkPosition{posX, posY, posZ};
}
