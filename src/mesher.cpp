#include <GLFW/glfw3.h>
#include "chunk.h"
#include "glm/geometric.hpp"
#include "mesher.h"

shared_ptr<spdlog::logger> Mesher::logger = shared_ptr<spdlog::logger>();
Mesher::Mesher() {
  if(!logger) {
    logger = make_shared<spdlog::logger>("Mesher", fileSink);
  }
  logger->set_level(spdlog::level::debug);
}

ChunkMesh Mesher::simpleMesh(int chunkX, int chunkZ, Chunk *chunk) {
  ChunkMesh rv;
  rv.type = SIMPLE;
  auto size = chunk->getSize();

  int totalSize = size[0] * size[1] * size[2];
  glm::vec3 offset(size[0]*chunkX, 0, size[2]*chunkZ);
  ChunkCoords neighborCoords;
  Cube* neighbor;
  for(int i = 0; i<totalSize; i++) {
    if(chunk->data[i] != NULL) {
      ChunkCoords ci = chunk->getCoords(i);
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
        neighbor = chunk->getCube_(neighborCoords.x, neighborCoords.y, neighborCoords.z);
        if(neighbor == NULL) {
          for(int vertex = 0; vertex < 6; vertex++) {
            rv.positions.push_back(glm::vec3(ci.x, ci.y, ci.z) + faceModels[neighborIndex][vertex] + offset);
            rv.texCoords.push_back(texModels[neighborIndex][vertex]);
            rv.blockTypes.push_back(chunk->data[i]->blockType());
            rv.selects.push_back(chunk->data[i]->selected());
          }
        }
      }
    }
  }
  return rv;
}

ChunkMesh Mesher::meshGreedy(int chunkX, int chunkZ, Chunk* chunk) {
  double currentTime = glfwGetTime();
  ChunkMesh mesh;
  mesh.type = GREEDY;
  int i, j, k, l, w, h, u, v;
  int x[3];
  int q[3];
  int du[3];
  int dv[3];
  bool blockCurrent, blockCompare, done;
  int chunkOffset[3] = {chunkX * chunk->getSize()[0], 0, chunkZ*chunk->getSize()[2]};
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

    bool mask[chunkSizes[0]*chunkSizes[1]*chunkSizes[2]];
    // mask needs to become a 2d array of bools
    // mask[pos][blockType]

    q[dimension] = 1;

    // Check each slice of the chunk one at a time
    for (x[dimension] = -1; x[dimension] < chunkSizes[dimension];) {
      // Compute the mask
      int n = 0;
      for (x[v] = 0; x[v] < chunkSizes[v]; ++x[v]) {
        for (x[u] = 0; x[u] < chunkSizes[u]; ++x[u]) {
          Cube *a = chunk->getCube_(x[0], x[1], x[2]);
          Cube *b = chunk->getCube_(x[0] + q[0], x[1] + q[1], x[2] + q[2]);
          blockCurrent =
            0 <= x[dimension] ? a != NULL
            : false;

          blockCompare =
              x[dimension] < chunkSizes[dimension] - 1
                  ? b != NULL
                  : false;

          mask[n++] = blockCurrent != blockCompare;
        }
      }

      ++x[dimension];

      n = 0;

      // Generate a mesh from the mask using lexicographic ordering,
      //   by looping over each block in this slice of the chunk
      for (j = 0; j < chunkSizes[v]; ++j) {
        for (i = 0; i < chunkSizes[u];) {
          if (mask[n]) {

            x[u] = i;
            x[v] = j;

            Cube *c = chunk->getCube_(x[0] - q[0], x[1] - q[1], x[2] - q[2]);
            if(c == NULL) {
              c = chunk->getCube_(x[0] , x[1] , x[2]);
            }

            // Compute the width of this quad and store it in w
            //   This is done by searching along the current axis until mask[n +
            //   w] is false
            for (w = 1; i + w < chunkSizes[u]; w++) {
              int tmp = x[u];
              x[u] = x[u] + w;
              Cube *next = chunk->getCube_(x[0] - q[0], x[1] - q[1], x[2] - q[2]);
              if (next == NULL) {
                next = chunk->getCube_(x[0], x[1], x[2]);
              }
              x[u] = tmp;
              if(!mask[n + w] || (next!=NULL && c!=NULL && next->blockType() != c->blockType())) {
                break;
              }
            }

            // Compute the height of this quad and store it in h
            //   This is done by checking if every block next to this row (range
            //   0 to w) is also part of the mask. For example, if w is 5 we
            //   currently have a quad of dimensions 1 x 5. To reduce triangle
            //   count, greedy meshing will attempt to expand this quad out to
            //   CHUNK_SIZE x 5, but will stop if it reaches a hole in the mask

            done = false;
            for (h = 1; j + h < chunkSizes[v]; h++) {
              // Check each block next to this quad
              for (k = 0; k < w; ++k) {
                // If there's a hole in the mask, exit

                int tmp = x[v];
                x[v] = x[v] + h;
                Cube *next =
                    chunk->getCube_(x[0] - q[0], x[1] - q[1], x[2] - q[2]);
                if (next == NULL) {
                  next = chunk->getCube_(x[0], x[1], x[2]);
                }
                x[v] = tmp;

                if (!mask[n + k + h * chunkSizes[u]] ||
                    (next != NULL && c != NULL &&
                     next->blockType() != c->blockType())) {
                  done = true;
                  break;
                }
              }

              if (done)
                break;
            }
            // du and dv determine the size and orientation of this face
            du[0] = 0;
            du[1] = 0;
            du[2] = 0;
            du[u] = w;

            dv[0] = 0;
            dv[1] = 0;
            dv[2] = 0;
            dv[v] = h;

            // Create a quad for this face. Colour, normal or textures are not
            // stored in this block vertex format.

            glm::vec3 offset = glm::vec3(-0.5,-0.5,-0.5) + glm::vec3(chunkOffset[0], chunkOffset[1], chunkOffset[2]);

            mesh.positions.push_back(offset+glm::vec3(x[0], x[1], x[2]));
            mesh.positions.push_back(offset+glm::vec3(x[0] + du[0], x[1] + du[1], x[2] + du[2]));
            mesh.positions.push_back(offset+glm::vec3(x[0] + dv[0], x[1] + dv[1], x[2] + dv[2]));

            mesh.positions.push_back(offset+glm::vec3(x[0] + du[0], x[1] + du[1], x[2] + du[2]));
            mesh.positions.push_back(offset+glm::vec3(x[0] + du[0] + dv[0], x[1] + du[1] + dv[1], x[2] + du[2] + dv[2]));
            mesh.positions.push_back(offset+
                glm::vec3(x[0] + dv[0], x[1] + dv[1], x[2] + dv[2]));


            for(int i = 0; i < 6; i++) {
              if(c != NULL) {
                // until I fix the bounding box, I need to test for presense of cube
                mesh.blockTypes.push_back(c->blockType());
              } else {
                mesh.blockTypes.push_back(0);
              }
              mesh.selects.push_back(0);
            }
            float yTexDist = glm::distance(
                glm::vec3(x[0],x[1],x[2]),
                glm::vec3(x[0]+du[0], x[1]+du[1], x[2]+du[2]));

            float xTexDist = glm::distance(
                glm::vec3(x[0], x[1], x[2]),
                glm::vec3(x[0] + dv[0], x[1] + dv[1], x[2] + dv[2]));

            mesh.texCoords.push_back(glm::vec2(0.0f, 0.0f));
            mesh.texCoords.push_back(glm::vec2(0.0f, yTexDist));
            mesh.texCoords.push_back(glm::vec2(xTexDist, 0.0f));

            mesh.texCoords.push_back(glm::vec2(0.0f, yTexDist));
            mesh.texCoords.push_back(glm::vec2(xTexDist, yTexDist));
            mesh.texCoords.push_back(glm::vec2(xTexDist, 0.0f));

            // Clear this part of the mask, so we don't add duplicate faces
            for (l = 0; l < h; ++l)
              for (k = 0; k < w; ++k)
                mask[n + k + l * chunkSizes[u]] = false;

            // Increment counters and continue
            i += w;
            n += w;
          } else {
            i++;
            n++;
          }
        }
      }
    }
  }

  logger->debug("time:" + to_string(glfwGetTime()-currentTime));
  logger->flush();
  return mesh;
}


ChunkMesh Mesher::meshedFaceFromPosition(Chunk *chunk, Position position) {
  ChunkMesh rv;
  Cube *c = chunk->getCube_(position.x, position.y, position.z);
  if (c != NULL) {
    Face face = getFaceFromNormal(position.normal);
    vector<glm::vec3> offsets = getOffsetsFromFace(face);
    vector<glm::vec2> texCoords = getTexCoordsFromFace(face);
    assert(offsets.size() == texCoords.size());
    auto size = chunk->getSize();
    for (int i = 0; i < offsets.size(); i++) {
      rv.positions.push_back(
          offsets[i] + glm::vec3(position.x, position.y, position.z) +
          glm::vec3(chunk->posX * size[0], chunk->posY * size[1], chunk->posZ * size[2]));
      rv.blockTypes.push_back(c->blockType());
      rv.selects.push_back(c->selected());
      rv.texCoords.push_back(texCoords[i]);
    }
  }
  return rv;
}

vector<glm::vec2> Mesher::getTexCoordsFromFace(Face face) {
  int index = findNeighborFaceIndex(face);
  glm::vec2 *coords = texModels[index];
  vector<glm::vec2> rv;
  for (int i = 0; i < 6; i++) {
    rv.push_back(coords[i]);
  }
  return rv;
}

int Mesher::findNeighborFaceIndex(Face face) {
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

vector<glm::vec3> Mesher::getOffsetsFromFace(Face face) {
  int index = findNeighborFaceIndex(face);
  glm::vec3 *offsets = faceModels[index];
  vector<glm::vec3> rv;
  for (int i = 0; i < 6; i++) {
    rv.push_back(offsets[i]);
  }
  return rv;
}

Face Mesher::getFaceFromNormal(glm::vec3 normal) {
  // TBI
  if (normal.x < 0)
    return LEFT;
  if (normal.x > 0)
    return RIGHT;
  if (normal.y < 0)
    return BOTTOM;
  if (normal.y > 0)
    return TOP;
  if (normal.z < 0)
    return FRONT;
  if (normal.z > 0)
    return BACK;

  // null vector
  assert(false);
  return FRONT;
}


glm::vec2 Mesher::texModels[6][6] = {
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

Face Mesher::neighborFaces[] = {FRONT, BACK, LEFT, RIGHT, BOTTOM, TOP};

glm::vec3 Mesher::faceModels[6][6] = {

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

