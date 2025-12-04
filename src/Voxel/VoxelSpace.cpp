#include "Voxel/VoxelSpace.h"

#include <array>
#include <cmath>
#include <tuple>
#include <utility>

#include <glad/glad.h>
#include <glm/glm.hpp>

Voxel::Voxel() = default;

Voxel::Voxel(glm::vec3 position, float size)
  : position(position)
  , size(size)
{}

std::pair<std::vector<glm::vec3>, std::vector<glm::vec3>>
Voxel::buildVertices() const
{
  const float half = size * 0.5f;
  const std::array<glm::vec3, 8> corners = {
    glm::vec3{ -half, -half, -half }, glm::vec3{ half, -half, -half },
    glm::vec3{ half, half, -half },   glm::vec3{ -half, half, -half },
    glm::vec3{ -half, -half, half },  glm::vec3{ half, -half, half },
    glm::vec3{ half, half, half },    glm::vec3{ -half, half, half }
  };

  const std::array<std::array<int, 6>, 6> faces = {
    std::array<int, 6>{ 0, 1, 2, 2, 3, 0 }, // back
    std::array<int, 6>{ 4, 5, 6, 6, 7, 4 }, // front
    std::array<int, 6>{ 0, 4, 7, 7, 3, 0 }, // left
    std::array<int, 6>{ 1, 5, 6, 6, 2, 1 }, // right
    std::array<int, 6>{ 3, 2, 6, 6, 7, 3 }, // top
    std::array<int, 6>{ 0, 1, 5, 5, 4, 0 }  // bottom
  };

  const std::array<glm::vec3, 6> faceNormals = {
    glm::vec3{ 0, 0, -1 }, glm::vec3{ 0, 0, 1 }, glm::vec3{ -1, 0, 0 },
    glm::vec3{ 1, 0, 0 },  glm::vec3{ 0, 1, 0 }, glm::vec3{ 0, -1, 0 }
  };

  const std::array<glm::vec3, 6> triBarycentrics = {
    glm::vec3{ 1, 0, 0 }, glm::vec3{ 0, 1, 0 }, glm::vec3{ 0, 0, 1 },
    glm::vec3{ 1, 0, 0 }, glm::vec3{ 0, 1, 0 }, glm::vec3{ 0, 0, 1 }
  };

  std::vector<glm::vec3> vertices;
  std::vector<glm::vec3> barycentrics;
  vertices.reserve(36);
  barycentrics.reserve(36);
  for (size_t faceIndex = 0; faceIndex < faces.size(); ++faceIndex) {
    const auto& face = faces[faceIndex];
    for (size_t v = 0; v < face.size(); ++v) {
      int index = face[v];
      vertices.push_back(position + corners[index]);
      barycentrics.push_back(triBarycentrics[v]);
    }
  }

  return { vertices, barycentrics };
}

RenderedVoxelSpace::RenderedVoxelSpace() = default;

RenderedVoxelSpace::RenderedVoxelSpace(
  const std::vector<glm::vec3>& positions,
  const std::vector<glm::vec3>& barycentrics)
{
  upload(positions, barycentrics);
}

RenderedVoxelSpace::RenderedVoxelSpace(RenderedVoxelSpace&& other) noexcept
  : vao(std::exchange(other.vao, 0))
  , vboPositions(std::exchange(other.vboPositions, 0))
  , vboBarycentrics(std::exchange(other.vboBarycentrics, 0))
  , vertexCount(other.vertexCount)
{
  other.vertexCount = 0;
}

RenderedVoxelSpace&
RenderedVoxelSpace::operator=(RenderedVoxelSpace&& other) noexcept
{
  if (this != &other) {
    destroy();
    vao = std::exchange(other.vao, 0);
    vboPositions = std::exchange(other.vboPositions, 0);
    vboBarycentrics = std::exchange(other.vboBarycentrics, 0);
    vertexCount = other.vertexCount;
    other.vertexCount = 0;
  }
  return *this;
}

RenderedVoxelSpace::~RenderedVoxelSpace()
{
  destroy();
}

void
RenderedVoxelSpace::upload(const std::vector<glm::vec3>& positions,
                           const std::vector<glm::vec3>& barycentrics)
{
  destroy();
  vertexCount = static_cast<int>(positions.size());
  if (vertexCount == 0 || barycentrics.size() != positions.size()) {
    return;
  }

  glGenVertexArrays(1, &vao);
  glGenBuffers(1, &vboPositions);
  glGenBuffers(1, &vboBarycentrics);

  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, vboPositions);
  glBufferData(GL_ARRAY_BUFFER,
               positions.size() * sizeof(glm::vec3),
               positions.data(),
               GL_STATIC_DRAW);

  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);

  // normals unused for voxels; supply a constant normal so the shader gets a
  // stable value without needing a buffer.
  glDisableVertexAttribArray(2);
  glVertexAttrib3f(2, 0.0f, 1.0f, 0.0f);

  glBindBuffer(GL_ARRAY_BUFFER, vboBarycentrics);
  glBufferData(GL_ARRAY_BUFFER,
               barycentrics.size() * sizeof(glm::vec3),
               barycentrics.data(),
               GL_STATIC_DRAW);
  glEnableVertexAttribArray(7);
  glVertexAttribPointer(7, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);

  glBindVertexArray(0);
}

void
RenderedVoxelSpace::draw() const
{
  if (vao == 0 || vertexCount == 0) {
    return;
  }
  glBindVertexArray(vao);
  glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertexCount));
  glBindVertexArray(0);
}

void
RenderedVoxelSpace::destroy()
{
  if (vboPositions != 0) {
    glDeleteBuffers(1, &vboPositions);
    vboPositions = 0;
  }
  if (vboBarycentrics != 0) {
    glDeleteBuffers(1, &vboBarycentrics);
    vboBarycentrics = 0;
  }
  if (vao != 0) {
    glDeleteVertexArrays(1, &vao);
    vao = 0;
  }
  vertexCount = 0;
}

void
VoxelSpace::add(glm::vec3 position, float size)
{
  voxels.emplace_back(position, size);
}

bool
VoxelSpace::has(glm::vec3 position, float size) const
{
  const float EPS = 0.0001f;
  for (const auto& v : voxels) {
    if (fabs(v.getSize() - size) < EPS &&
        glm::length(v.getPosition() - position) < EPS) {
      return true;
    }
  }
  return false;
}

RenderedVoxelSpace
VoxelSpace::render() const
{
  std::vector<glm::vec3> vertices;
  std::vector<glm::vec3> barycentrics;
  vertices.reserve(voxels.size() * 36);
  barycentrics.reserve(voxels.size() * 36);
  for (const auto& voxel : voxels) {
    auto mesh = voxel.buildVertices();
    auto& v = mesh.first;
    auto& b = mesh.second;
    vertices.insert(vertices.end(), v.begin(), v.end());
    barycentrics.insert(barycentrics.end(), b.begin(), b.end());
  }
  return RenderedVoxelSpace(vertices, barycentrics);
}

void
VoxelSpace::clear()
{
  voxels.clear();
}
