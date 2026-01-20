#include <GLFW/glfw3.h>
#include <future>
#include <memory>
#include "time_utils.h"
#include "chunk.h"
#include "glm/geometric.hpp"
#include "mesher.h"

shared_ptr<ChunkMesh>
Mesher::simpleMesh(Chunk* chunk)
{
  auto rv = make_shared<ChunkMesh>(ChunkMesh());
  rv->type = SIMPLE;
  auto size = chunk->getSize();

  int totalSize = size[0] * size[1] * size[2];
  glm::vec3 offset(size[0] * chunkX, 0, size[2] * chunkZ);
  ChunkCoords neighborCoords;
  shared_ptr<Cube> neighbor;
  for (int i = 0; i < totalSize; i++) {
    if (chunk->data[i] != NULL) {
      ChunkCoords ci = chunk->getCoords(i);
      ChunkCoords neighbors[6] = {
        ChunkCoords{ ci.x, ci.y, ci.z - 1 },
        ChunkCoords{ ci.x, ci.y, ci.z + 1 },
        ChunkCoords{ ci.x - 1, ci.y, ci.z },
        ChunkCoords{ ci.x + 1, ci.y, ci.z },
        ChunkCoords{ ci.x, ci.y - 1, ci.z },
        ChunkCoords{ ci.x, ci.y + 1, ci.z },
      };

      for (int neighborIndex = 0; neighborIndex < 6; neighborIndex++) {
        neighborCoords = neighbors[neighborIndex];
        neighbor =
          chunk->getCube_(neighborCoords.x, neighborCoords.y, neighborCoords.z);
        if (neighbor == NULL) {
          for (int vertex = 0; vertex < 6; vertex++) {
            rv->positions.push_back(glm::vec3(ci.x, ci.y, ci.z) +
                                    faceModels[neighborIndex][vertex] + offset);
            rv->texCoords.push_back(texModels[neighborIndex][vertex]);
            rv->blockTypes.push_back(chunk->data[i]->blockType());
            rv->selects.push_back(chunk->data[i]->selected());
          }
        }
      }
    }
  }
  return rv;
}

PartitionedChunkMeshes
Mesher::meshGreedy(Chunk* chunk)
{
  double currentTime = nowSeconds();
  PartitionedChunkMeshes meshes;
  int i, j, k, l, w, h, u, v;
  int x[3];
  int q[3];
  int du[3];
  int dv[3];
  bool blockCurrent, blockCompare, done;
  int chunkOffset[3] = { chunkX * chunk->getSize()[0],
                         0,
                         chunkZ * chunk->getSize()[2] };

  auto partitions = partitioner.partition(chunk);

  int partitionNo = 0;
  for (auto partition : partitions) {
    if (partitionsDamaged[partitionNo++]) {
      auto yOff = partition.y();
      auto mesh = make_shared<ChunkMesh>(ChunkMesh());
      mesh->type = GREEDY;
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

        array<int, 3> partitionSizes = partition.getSize();

        bool mask[partitionSizes[0] * partitionSizes[1] * partitionSizes[2]];

        q[dimension] = 1;

        // Check each slice of the chunk one at a time
        for (x[dimension] = -1; x[dimension] < partitionSizes[dimension];) {
          // Compute the mask
          int n = 0;
          for (x[v] = 0; x[v] < partitionSizes[v]; ++x[v]) {
            for (x[u] = 0; x[u] < partitionSizes[u]; ++x[u]) {
              shared_ptr<Cube> a = chunk->getCube_(x[0], x[1] + yOff, x[2]);
              shared_ptr<Cube> b =
                chunk->getCube_(x[0] + q[0], x[1] + q[1] + yOff, x[2] + q[2]);
              blockCurrent = 0 <= x[dimension] ? a != NULL : false;

              blockCompare = x[dimension] < partitionSizes[dimension] - 1
                               ? b != NULL
                               : false;

              // only one face is valid
              // I will want to check block opacity
              // If 1 block is transparent and another isn't
              // then a face (maybe both) should be rendered
              mask[n++] = blockCurrent != blockCompare;
            }
          }

          ++x[dimension];

          n = 0;

          // Generate a mesh from the mask using lexicographic ordering,
          //   by looping over each block in this slice of the chunk
          for (j = 0; j < partitionSizes[v]; ++j) {
            for (i = 0; i < partitionSizes[u];) {
              if (mask[n]) {

                x[u] = i;
                x[v] = j;

                shared_ptr<Cube> c =
                  chunk->getCube_(x[0] - q[0], x[1] - q[1] + yOff, x[2] - q[2]);
                if (c == NULL) {
                  c = chunk->getCube_(x[0], x[1] + yOff, x[2]);
                }
                assert(c != NULL);

                // Compute the width of this quad and store it in w
                //   This is done by searching along the current axis until
                //   mask[n
                //   + w] is false
                for (w = 1; i + w < partitionSizes[u]; w++) {
                  int tmp = x[u];
                  x[u] = x[u] + w;
                  shared_ptr<Cube> next = chunk->getCube_(
                    x[0] - q[0], x[1] - q[1] + yOff, x[2] - q[2]);
                  if (next == NULL) {
                    next = chunk->getCube_(x[0], x[1] + yOff, x[2]);
                  }
                  x[u] = tmp;

                  if (!mask[n + w] || next->blockType() != c->blockType()) {
                    break;
                  }
                }

                // Compute the height of this quad and store it in h
                //   This is done by checking if every block next to this row
                //   (range 0 to w) is also part of the mask. For example, if w
                //   is 5 we currently have a quad of dimensions 1 x 5. To
                //   reduce triangle count, greedy meshing will attempt to
                //   expand this quad out to CHUNK_SIZE x 5, but will stop if it
                //   reaches a hole in the mask

                done = false;
                for (h = 1; j + h < partitionSizes[v]; h++) {
                  // Check each block next to this quad
                  for (k = 0; k < w; ++k) {
                    // If there's a hole in the mask, exit

                    int tmp = x[v];
                    x[v] = x[v] + h;
                    shared_ptr<Cube> next = chunk->getCube_(
                      x[0] - q[0], x[1] - q[1] + yOff, x[2] - q[2]);
                    if (next == NULL) {
                      next = chunk->getCube_(x[0], x[1] + yOff, x[2]);
                    }
                    x[v] = tmp;

                    if (!mask[n + k + h * partitionSizes[u]] ||
                        next->blockType() != c->blockType()) {
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

                // Create a quad for this face. Colour, normal or textures are
                // not stored in this block vertex format.

                glm::vec3 offset =
                  glm::vec3(-0.5, -0.5, -0.5) +
                  glm::vec3(chunkOffset[0], chunkOffset[1], chunkOffset[2]) +
                  glm::vec3(0, yOff, 0);

                mesh->positions.push_back(offset + glm::vec3(x[0], x[1], x[2]));
                mesh->positions.push_back(
                  offset + glm::vec3(x[0] + du[0], x[1] + du[1], x[2] + du[2]));
                mesh->positions.push_back(
                  offset + glm::vec3(x[0] + dv[0], x[1] + dv[1], x[2] + dv[2]));

                mesh->positions.push_back(
                  offset + glm::vec3(x[0] + du[0], x[1] + du[1], x[2] + du[2]));
                mesh->positions.push_back(offset +
                                          glm::vec3(x[0] + du[0] + dv[0],
                                                    x[1] + du[1] + dv[1],
                                                    x[2] + du[2] + dv[2]));
                mesh->positions.push_back(
                  offset + glm::vec3(x[0] + dv[0], x[1] + dv[1], x[2] + dv[2]));

                for (int i = 0; i < 6; i++) {
                  mesh->blockTypes.push_back(c->blockType());
                  mesh->selects.push_back(0);
                }
                float yTexDist = glm::distance(
                  glm::vec3(x[0], x[1], x[2]),
                  glm::vec3(x[0] + du[0], x[1] + du[1], x[2] + du[2]));

                float xTexDist = glm::distance(
                  glm::vec3(x[0], x[1], x[2]),
                  glm::vec3(x[0] + dv[0], x[1] + dv[1], x[2] + dv[2]));

                mesh->texCoords.push_back(glm::vec2(0.0f, 0.0f));
                mesh->texCoords.push_back(glm::vec2(0.0f, yTexDist));
                mesh->texCoords.push_back(glm::vec2(xTexDist, 0.0f));

                mesh->texCoords.push_back(glm::vec2(0.0f, yTexDist));
                mesh->texCoords.push_back(glm::vec2(xTexDist, yTexDist));
                mesh->texCoords.push_back(glm::vec2(xTexDist, 0.0f));

                // Clear this part of the mask, so we don't add duplicate faces
                for (l = 0; l < h; ++l)
                  for (k = 0; k < w; ++k)
                    mask[n + k + l * partitionSizes[u]] = false;

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
      meshes.push_back(mesh);
    } else {
      auto emptyMesh = make_shared<ChunkMesh>();
      meshes.push_back(emptyMesh);
    }
  }

  return meshes;
}

ChunkMesh
Mesher::meshedFaceFromPosition(Position position)
{
  ChunkMesh rv;
  shared_ptr<Cube> c = chunk->getCube_(position.x, position.y, position.z);
  if (c != NULL) {
    Face face = getFaceFromNormal(position.normal);
    vector<glm::vec3> offsets = getOffsetsFromFace(face);
    vector<glm::vec2> texCoords = getTexCoordsFromFace(face);
    assert(offsets.size() == texCoords.size());
    auto size = chunk->getSize();
    for (int i = 0; i < offsets.size(); i++) {
      rv.positions.push_back(
        offsets[i] + glm::vec3(position.x, position.y, position.z) +
        glm::vec3(chunkX * size[0], 0 * size[1], chunkZ * size[2]));
      rv.blockTypes.push_back(c->blockType());
      rv.selects.push_back(c->selected());
      rv.texCoords.push_back(texCoords[i]);
    }
  }
  return rv;
}

shared_ptr<ChunkMesh>
Mesher::mesh()
{
  PartitionedChunkMeshes completeSet =
    vector<shared_ptr<ChunkMesh>>(partitionsDamaged.size());
  auto promisedCache = cachedGreedyMesh.get();
  for (int i = 0; i < partitionsDamaged.size(); i++) {
    if (i < promisedCache.size()) {
      completeSet[i] = promisedCache[i];
    }
  }
  if (damagedGreedy) {
    auto mesh = make_shared<ChunkMesh>();
    PartitionedChunkMeshes meshes = meshGreedy(chunk);
    promise<PartitionedChunkMeshes> promisedMeshes;
    for (int i = 0; i < partitionsDamaged.size(); i++) {
      if (partitionsDamaged[i]) {
        partitionsDamaged[i] = false;
        completeSet[i] = meshes[i];
      }
    }
    damagedGreedy = false;
    promisedMeshes.set_value(completeSet);
    cachedGreedyMesh = promisedMeshes.get_future();
  }
  // no damage, no update, just use cache
  return mergePartitionedChunkMeshes(completeSet);
}

void
Mesher::meshAsync()
{
  Chunk* copiedChunk = new Chunk(*chunk);
  cachedGreedyMesh =
    async(launch::async, [copiedChunk, this]() -> PartitionedChunkMeshes {
      auto rv = meshGreedy(copiedChunk);
      delete copiedChunk;
      return rv;
    });
}

void
Mesher::meshDamaged(array<int, 3> pos)
{
  damagedSimple = true;
  damagedGreedy = true;
  int partitionDamaged = pos[1] / partitioner.getPartitionHeight();
  partitionsDamaged[partitionDamaged] = true;
}

Mesher::Mesher(Chunk* chunk, int chunkX, int chunkZ)
  : chunk(chunk)
  , chunkX(chunkX)
  , chunkZ(chunkZ)
{
  promise<PartitionedChunkMeshes> emptyMesh;
  auto mesh = make_shared<ChunkMesh>();
  vector<shared_ptr<ChunkMesh>> meshes;
  meshes.push_back(mesh);
  emptyMesh.set_value(meshes);
  cachedGreedyMesh = emptyMesh.get_future();
  damagedGreedy = false;
}

vector<glm::vec2>
Mesher::getTexCoordsFromFace(Face face)
{
  int index = findNeighborFaceIndex(face);
  glm::vec2* coords = texModels[index];
  vector<glm::vec2> rv;
  for (int i = 0; i < 6; i++) {
    rv.push_back(coords[i]);
  }
  return rv;
}

int
Mesher::findNeighborFaceIndex(Face face)
{
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

vector<glm::vec3>
Mesher::getOffsetsFromFace(Face face)
{
  int index = findNeighborFaceIndex(face);
  glm::vec3* offsets = faceModels[index];
  vector<glm::vec3> rv;
  for (int i = 0; i < 6; i++) {
    rv.push_back(offsets[i]);
  }
  return rv;
}

Face
Mesher::getFaceFromNormal(glm::vec3 normal)
{
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

glm::vec2 Mesher::texModels[6][6] = { { glm::vec2(0.0f, 0.0f),
                                        glm::vec2(0.0f, 1.0f),
                                        glm::vec2(1.0f, 1.0f),
                                        glm::vec2(1.0f, 1.0f),
                                        glm::vec2(1.0f, 0.0f),
                                        glm::vec2(0.0f, 0.0f) },

                                      { glm::vec2(0.0f, 0.0f),
                                        glm::vec2(1.0f, 0.0f),
                                        glm::vec2(1.0f, 1.0f),
                                        glm::vec2(1.0f, 1.0f),
                                        glm::vec2(0.0f, 1.0f),
                                        glm::vec2(0.0f, 0.0f) },

                                      { glm::vec2(1.0f, 0.0f),
                                        glm::vec2(1.0f, 1.0f),
                                        glm::vec2(0.0f, 1.0f),
                                        glm::vec2(0.0f, 1.0f),
                                        glm::vec2(0.0f, 0.0f),
                                        glm::vec2(1.0f, 0.0f) },

                                      { glm::vec2(1.0f, 0.0f),
                                        glm::vec2(0.0f, 0.0f),
                                        glm::vec2(0.0f, 1.0f),
                                        glm::vec2(0.0f, 1.0f),
                                        glm::vec2(1.0f, 1.0f),
                                        glm::vec2(1.0f, 0.0f) },

                                      { glm::vec2(0.0f, 1.0f),
                                        glm::vec2(1.0f, 1.0f),
                                        glm::vec2(1.0f, 0.0f),
                                        glm::vec2(1.0f, 0.0f),
                                        glm::vec2(0.0f, 0.0f),
                                        glm::vec2(0.0f, 1.0f) },

                                      { glm::vec2(1.0f, 0.0f),
                                        glm::vec2(1.0f, 1.0f),
                                        glm::vec2(0.0f, 1.0f),
                                        glm::vec2(1.0f, 0.0f),
                                        glm::vec2(0.0f, 1.0f),
                                        glm::vec2(0.0f, 0.0f) } };

Face Mesher::neighborFaces[] = { FRONT, BACK, LEFT, RIGHT, BOTTOM, TOP };

glm::vec3 Mesher::faceModels[6][6] = {

  // front
  { glm::vec3(-0.5f, -0.5f, -0.5f),
    glm::vec3(-0.5f, 0.5f, -0.5f),
    glm::vec3(0.5f, 0.5f, -0.5f),
    glm::vec3(0.5f, 0.5f, -0.5f),
    glm::vec3(0.5f, -0.5f, -0.5f),
    glm::vec3(-0.5f, -0.5f, -0.5f) },

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
  { glm::vec3(-0.5f, 0.5f, 0.5f),
    glm::vec3(-0.5f, 0.5f, -0.5f),
    glm::vec3(-0.5f, -0.5f, -0.5f),
    glm::vec3(-0.5f, -0.5f, -0.5f),
    glm::vec3(-0.5f, -0.5f, 0.5f),
    glm::vec3(-0.5f, 0.5f, 0.5f) },

  // right
  { glm::vec3(0.5f, 0.5f, 0.5f),
    glm::vec3(0.5f, -0.5f, 0.5f),
    glm::vec3(0.5f, -0.5f, -0.5f),
    glm::vec3(0.5f, -0.5f, -0.5f),
    glm::vec3(0.5f, 0.5f, -0.5f),
    glm::vec3(0.5f, 0.5f, 0.5f) },

  // down
  { glm::vec3(-0.5f, -0.5f, -0.5f),
    glm::vec3(0.5f, -0.5f, -0.5f),
    glm::vec3(0.5f, -0.5f, 0.5f),
    glm::vec3(0.5f, -0.5f, 0.5f),
    glm::vec3(-0.5f, -0.5f, 0.5f),
    glm::vec3(-0.5f, -0.5f, -0.5f) },

  // up
  { glm::vec3(0.5f, 0.5f, 0.5f),
    glm::vec3(0.5f, 0.5f, -0.5f),
    glm::vec3(-0.5f, 0.5f, -0.5f),
    glm::vec3(0.5f, 0.5f, 0.5f),
    glm::vec3(-0.5f, 0.5f, -0.5f),
    glm::vec3(-0.5f, 0.5f, 0.5f) }
};

ChunkPartition::ChunkPartition(Chunk* chunk, int y, int ySize)
  : chunk(chunk)
  , _y(y)
  , ySize(ySize)
{
}
int
ChunkPartition::y()
{
  return _y;
}
array<int, 3>
ChunkPartition::getSize()
{
  array<int, 3> size;
  auto chunkSize = chunk->getSize();
  size[0] = chunkSize[0];
  size[1] = ySize;
  size[2] = chunkSize[2];
  return size;
}

ChunkPartitioner::ChunkPartitioner(unsigned int partitionHeight)
  : partitionHeight(partitionHeight)
{
}

vector<ChunkPartition>
ChunkPartitioner::partition(Chunk* chunk)
{
  vector<ChunkPartition> rv;
  for (int y = 0; y < chunk->getSize()[1]; y += partitionHeight) {
    // at the end, may need to be smaller than paritionHeight
    int height = min(chunk->getSize()[1] - y, int(partitionHeight));
    rv.push_back(ChunkPartition(chunk, y, height));
  }
  return rv;
}

shared_ptr<ChunkMesh>
Mesher::mergePartitionedChunkMeshes(PartitionedChunkMeshes meshes)
{
  auto rv = make_shared<ChunkMesh>();
  for (auto mesh : meshes) {
    for (auto position : mesh->positions) {
      rv->positions.push_back(position);
    }
    for (auto texCoord : mesh->texCoords) {
      rv->texCoords.push_back(texCoord);
    }
    for (auto blockType : mesh->blockTypes) {
      rv->blockTypes.push_back(blockType);
    }
    for (auto select : mesh->selects) {
      rv->selects.push_back(select);
    }
  }
  return rv;
}
