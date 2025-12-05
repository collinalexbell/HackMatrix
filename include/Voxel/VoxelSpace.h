#pragma once

#include <tuple>
#include <vector>

#include <glm/vec3.hpp>

class RenderedVoxelSpace;

class Voxel
{
public:
  Voxel();
  Voxel(glm::vec3 position, float size, glm::vec3 color);
  std::tuple<std::vector<glm::vec3>,
             std::vector<glm::vec3>,
             std::vector<glm::vec3>> buildVertices() const;
  glm::vec3 getPosition() const { return position; }
  float getSize() const { return size; }
  glm::vec3 getColor() const { return color; }

private:
  glm::vec3 position{ 0.0f };
  float size = 1.0f;
  glm::vec3 color{ 1.0f };
};

class RenderedVoxelSpace
{
public:
  RenderedVoxelSpace();
  explicit RenderedVoxelSpace(
    const std::vector<glm::vec3>& positions,
    const std::vector<glm::vec3>& barycentrics,
    const std::vector<glm::vec3>& colors);
  RenderedVoxelSpace(RenderedVoxelSpace&& other) noexcept;
  RenderedVoxelSpace& operator=(RenderedVoxelSpace&& other) noexcept;
  RenderedVoxelSpace(const RenderedVoxelSpace&) = delete;
  RenderedVoxelSpace& operator=(const RenderedVoxelSpace&) = delete;
  ~RenderedVoxelSpace();

  void upload(const std::vector<glm::vec3>& positions,
              const std::vector<glm::vec3>& barycentrics,
              const std::vector<glm::vec3>& colors);
  void draw() const;

private:
  void destroy();
  unsigned int vao = 0;
  unsigned int vboPositions = 0;
  unsigned int vboBarycentrics = 0;
  unsigned int vboColors = 0;
  int vertexCount = 0;
};

class VoxelSpace
{
public:
  void add(glm::vec3 position, float size = 1.0f, glm::vec3 color = glm::vec3(1.0f));
  bool has(glm::vec3 position, float size = 1.0f) const;
  size_t remove(glm::vec3 min, glm::vec3 max);
  RenderedVoxelSpace render() const;
  void clear();
  bool empty() const { return voxels.empty(); }

private:
  std::vector<Voxel> voxels;
};
