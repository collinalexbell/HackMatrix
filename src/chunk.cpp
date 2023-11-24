#include "chunk.h"

Face Chunk::neighborFaces[] = {LEFT, RIGHT, BOTTOM, TOP, FRONT, BACK};
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
  if(x>=0 && x < size[0] && y >=0 && y < size[1] &&  z >= 0 && z < size[2]) {
    return data[index(x, y, z)];
  }
  return NULL;
}

void Chunk::removeCube(int x, int y, int z) {
  delete data[index(x, y, z)];
  data[index(x, y, z)] = NULL;
}
void Chunk::addCube(Cube c, int x, int y, int z) {
  data[index(x, y, z)] = new Cube(c);
}

glm::vec2 texModels[6][6] = {
    {glm::vec2(0.0f, 0.0f), glm::vec2(0.0f, 1.0f), glm::vec2(1.0f, 1.0f),
     glm::vec2(1.0f, 1.0f), glm::vec2(1.0f, 0.0f), glm::vec2(0.0f, 0.0f)},

    {glm::vec2(0.0f, 0.0f), glm::vec2(1.0f, 0.0f), glm::vec2(1.0f, 1.0f),
     glm::vec2(1.0f, 1.0f), glm::vec2(0.0f, 1.0f), glm::vec2(0.0f, 0.0f)},

    {glm::vec2(1.0f, 0.0f), glm::vec2(1.0f, 1.0f), glm::vec2(0.0f, 1.0f),
     glm::vec2(0.0f, 1.0f), glm::vec2(0.0f, 0.0f), glm::vec2(1.0f, 0.0f)},

    {glm::vec2(1.0f, 0.0f), glm::vec2(0.0f, 0.0f), glm::vec2(0.0f, 1.0f),
     glm::vec2(0.0f, 1.0f), glm::vec2(1.0f, 1.0f), glm::vec2(1.0f, 0.0f)},

    {glm::vec2(0.0f, 1.0f), glm::vec2(1.0f, 1.0f), glm::vec2(1.0f, 0.0f),
     glm::vec2(1.0f, 0.0f), glm::vec2(0.0f, 0.0f), glm::vec2(0.0f, 1.0f)},

    {glm::vec2(1.0f, 0.0f), glm::vec2(1.0f, 1.0f), glm::vec2(0.0f, 1.0f),
     glm::vec2(1.0f, 0.0f), glm::vec2(0.0f, 1.0f), glm::vec2(0.0f, 0.0f)}
};

glm::vec3 faceModels[6][6] = {


  // front
  {
   glm::vec3(-0.5f, -0.5f, -0.5f),
   glm::vec3(-0.5f, 0.5f, -0.5f),
   glm::vec3(0.5f, 0.5f, -0.5f),
   glm::vec3(0.5f, 0.5f, -0.5f),
   glm::vec3(0.5f, -0.5f, -0.5f),
   glm::vec3(-0.5f, -0.5f, -0.5f)
  },

  // back
  {
    glm::vec3(-0.5f, -0.5f, 0.5f),
    glm::vec3(0.5f, -0.5f, 0.5f),
    glm::vec3(0.5f, 0.5f, 0.5f),
    glm::vec3(0.5f, 0.5f, 0.5f),
    glm::vec3(-0.5f, 0.5f, 0.5f),
    glm::vec3(-0.5f, -0.5f, 0.5f),
  },

  // left
  {
    glm::vec3(-0.5f, 0.5f, 0.5f),
    glm::vec3(-0.5f, 0.5f, -0.5f),
    glm::vec3(-0.5f, -0.5f, -0.5f),
    glm::vec3(-0.5f, -0.5f, -0.5f),
    glm::vec3(-0.5f, -0.5f, 0.5f),
    glm::vec3(-0.5f, 0.5f, 0.5f)
  },

  // right
  {
    glm::vec3(0.5f, 0.5f, 0.5f),
    glm::vec3(0.5f, -0.5f, 0.5f),
    glm::vec3(0.5f, -0.5f, -0.5f),
    glm::vec3(0.5f, -0.5f, -0.5f),
    glm::vec3(0.5f, 0.5f, -0.5f),
    glm::vec3(0.5f, 0.5f, 0.5f)
  },

  // down
  {
    glm::vec3(-0.5f, -0.5f, -0.5f),
    glm::vec3(0.5f, -0.5f, -0.5f),
    glm::vec3(0.5f, -0.5f, 0.5f),
    glm::vec3(0.5f, -0.5f, 0.5f),
    glm::vec3(-0.5f, -0.5f, 0.5f),
    glm::vec3(-0.5f, -0.5f, -0.5f)
  },

  // up
  {
    glm::vec3(0.5f, 0.5f, 0.5f),
    glm::vec3(0.5f, 0.5f, -0.5f),
    glm::vec3(-0.5f, 0.5f, -0.5f),
    glm::vec3(0.5f, 0.5f, 0.5f),
    glm::vec3(-0.5f, 0.5f, -0.5f),
    glm::vec3(-0.5f, 0.5f, 0.5f)
  }};

ChunkMesh Chunk::meshedFaceFromPosition(Position position) {
  ChunkMesh rv;
  return rv;
}

ChunkMesh Chunk::mesh() {
  ChunkMesh rv;
  int x,y,z;
  int totalSize = size[0] * size[1] * size[2];
  ChunkCoords neighborCoords;
  Cube* neighbor;
  for(int i = 0; i<totalSize; i++) {
    if(data[i] != NULL) {
      ChunkCoords ci = getCoords(i);
      ChunkCoords neighbors[6] = {
          ChunkCoords{ci.x, ci.y, ci.z - 1},
          ChunkCoords{ci.x, ci.y, ci.z + 1},
          ChunkCoords{ci.x - 1, ci.y, ci.z},
          ChunkCoords{ci.x + 1, ci.y, ci.z},
          ChunkCoords{ci.x, ci.y - 1, ci.z},
          ChunkCoords{ci.x, ci.y + 1, ci.z},
      };

      for(int neighborIndex = 0; neighborIndex < 6; neighborIndex++) {
        neighborCoords = neighbors[neighborIndex];
        Face face = neighborFaces[neighborIndex];
        neighbor = getCube_(neighborCoords.x, neighborCoords.y, neighborCoords.z);
        if(neighbor == NULL) {
          for(int vertex = 0; vertex < 6; vertex++) {
            rv.positions.push_back(glm::vec3(ci.x, ci.y, ci.z) + faceModels[neighborIndex][vertex]);
            rv.texCoords.push_back(texModels[neighborIndex][vertex]);
            rv.blockTypes.push_back(data[i]->blockType());
            rv.selects.push_back(data[i]->selected());
          }
        }
      }
    }
  }
  return rv;
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
