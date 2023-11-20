#include <GLFW/glfw3.h>
#include "mesher.h"

Mesher::Mesher() {
  logger = make_shared<spdlog::logger>("Mesher", fileSink);
  logger->set_level(spdlog::level::debug);
}

Mesh Mesher::meshGreedy(Chunk* chunk) {
  double currentTime = glfwGetTime();
  Mesh quads;
  int i, j, k, l, w, h, u, v;
  int x[3];
  int q[3];
  int du[3];
  int dv[3];
  bool blockCurrent, blockCompare, done;
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

    q[dimension] = 1;

    // Check each slice of the chunk one at a time
    for (x[dimension] = -1; x[dimension] < chunkSizes[dimension];) {
      // Compute the mask
      int n = 0;
      for (x[v] = 0; x[v] < chunkSizes[v]; ++x[v]) {
        for (x[u] = 0; x[u] < chunkSizes[u]; ++x[u]) {
          blockCurrent =
            0 <= x[dimension] ? chunk->getCube_(x[0], x[1], x[2]) != NULL
                        : true;
          blockCompare =
              x[dimension] < chunkSizes[dimension] - 1
                  ? chunk->getCube_(x[0] + q[0], x[1] + q[1], x[2] + q[2]) != NULL
                  : true;
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
            // Compute the width of this quad and store it in w
            //   This is done by searching along the current axis until mask[n +
            //   w] is false
            for (w = 1; i + w < chunkSizes[u] && mask[n + w]; w++) {
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
                if (!mask[n + k + h * chunkSizes[u]]) {
                  done = true;
                  break;
                }
              }

              if (done)
                break;
            }

            x[u] = i;
            x[v] = j;

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

            quads.push_back(glm::vec3(x[0], x[1], x[2]));
            quads.push_back(glm::vec3(x[0] + du[0], x[1] + du[1], x[2] + du[2]));
            quads.push_back(glm::vec3(x[0] + dv[0], x[1] + dv[1], x[2] + dv[2]));
            quads.push_back(glm::vec3(x[0] + du[0] + dv[0],
                                      x[1] + du[1] + dv[1],
                                      x[2] + du[2] + dv[2]));

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

  return quads;
}
