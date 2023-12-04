#include "chunk.h"
#include <cassert>

Chunk::Chunk(int x, int y, int z): posX(x), posY(y), posZ(z) {
  data = make_unique<Cube* []>(size[0] * size[1] * size[2]);
  for(int i=0; i < size[0]*size[1]*size[2]; i++) {
    data[i] = NULL;
  }
}

Chunk::Chunk() {
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
  delete data[index(x, y, z)];
  data[index(x, y, z)] = NULL;
}
void Chunk::addCube(Cube c, int x, int y, int z) {
  data[index(x, y, z)] = new Cube(c);
}

glm::vec2 Chunk::texModels[6][6] = {
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

Face Chunk::neighborFaces[] = {FRONT, BACK, LEFT, RIGHT, BOTTOM, TOP};

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

Face Chunk::getFaceFromNormal(glm::vec3 normal) {
  // TBI
  if(normal.x < 0) return LEFT;
  if(normal.x > 0) return RIGHT;
  if(normal.y < 0) return BOTTOM;
  if(normal.y > 0) return TOP;
  if(normal.z < 0) return FRONT;
  if(normal.z > 0) return BACK;

  // null vector
  assert(false);
  return FRONT;
}

int Chunk::findNeighborFaceIndex(Face face) {
  int index = 0;
  for (index = 0; index < 6; index++) {
    if (neighborFaces[index] == face) {
      break;
    }
  }

  // neighborFaces should have all Face enums
  assert(index != 6);

  return index;
}

vector<glm::vec3> Chunk::getOffsetsFromFace(Face face) {
  int index = findNeighborFaceIndex(face);
  glm::vec3 *offsets = faceModels[index];
  vector<glm::vec3> rv;
  for(int i = 0; i < 6; i++) {
    rv.push_back(offsets[i]);
  }
  return rv;
}

vector<glm::vec2> Chunk::getTexCoordsFromFace(Face face) {
  int index = findNeighborFaceIndex(face);
  glm::vec2 *coords = texModels[index];
  vector<glm::vec2> rv;
  for (int i = 0; i < 6; i++) {
    rv.push_back(coords[i]);
  }
  return rv;
}

ChunkMesh Chunk::meshedFaceFromPosition(Position position) {
  ChunkMesh rv;
  Cube *c = getCube_(position.x, position.y, position.z);
  if(c != NULL) {
    Face face = getFaceFromNormal(position.normal);
    vector<glm::vec3> offsets = getOffsetsFromFace(face);
    vector<glm::vec2> texCoords = getTexCoordsFromFace(face);
    assert(offsets.size() == texCoords.size());
    for(int i = 0; i < offsets.size(); i++) {
      rv.positions.push_back(offsets[i] + glm::vec3(position.x, position.y, position.z));
      rv.blockTypes.push_back(c->blockType());
      rv.selects.push_back(c->selected());
      rv.texCoords.push_back(texCoords[i]);
    }
  }
  return rv;
}

ChunkMesh Chunk::mesh() {
  ChunkMesh rv;
  int totalSize = size[0] * size[1] * size[2];
  glm::vec3 offset(posX*size[0], posY*size[1], posZ*size[2]);
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
        neighbor = getCube_(neighborCoords.x, neighborCoords.y, neighborCoords.z);
        if(neighbor == NULL) {
          for(int vertex = 0; vertex < 6; vertex++) {
            rv.positions.push_back(glm::vec3(ci.x, ci.y, ci.z) + faceModels[neighborIndex][vertex] + offset);
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
