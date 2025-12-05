#pragma once

#include <tuple>
#include <vector>

#include <glm/vec3.hpp>

class RenderedVoxelSpace;

class Voxel
{
public:
  Voxel();
  Voxel(glm::vec3 position, float size);
  std::pair<std::vector<glm::vec3>, std::vector<glm::vec3>> buildVertices() const;
  glm::vec3 getPosition() const { return position; }
  float getSize() const { return size; }

private:
  glm::vec3 position{ 0.0f };
  float size = 1.0f;
};

class RenderedVoxelSpace
{
public:
  RenderedVoxelSpace();
  explicit RenderedVoxelSpace(
    const std::vector<glm::vec3>& positions,
    const std::vector<glm::vec3>& barycentrics);
  RenderedVoxelSpace(RenderedVoxelSpace&& other) noexcept;
  RenderedVoxelSpace& operator=(RenderedVoxelSpace&& other) noexcept;
  RenderedVoxelSpace(const RenderedVoxelSpace&) = delete;
  RenderedVoxelSpace& operator=(const RenderedVoxelSpace&) = delete;
  ~RenderedVoxelSpace();

  void upload(const std::vector<glm::vec3>& positions,
              const std::vector<glm::vec3>& barycentrics);
  void draw() const;

private:
  void destroy();
  unsigned int vao = 0;
  unsigned int vboPositions = 0;
  unsigned int vboBarycentrics = 0;
  int vertexCount = 0;
};

class VoxelSpace
{
public:
  void add(glm::vec3 position, float size = 1.0f);
  bool has(glm::vec3 position, float size = 1.0f) const;
  RenderedVoxelSpace render() const;
  void clear();

private:
  std::vector<Voxel> voxels;
};
