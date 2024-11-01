#include "fakeit.hpp"

#include "chunk.h"
#include <gtest/gtest.h>

TEST(CHUNK, constructor) {
  ASSERT_NO_THROW(Chunk(0, 0, 0));
}

TEST(CHUNK, getPosition) {
  auto chunk = Chunk(0,1,2);
  auto position = chunk.getPosition();
  ASSERT_EQ(position.x, 0);
  ASSERT_EQ(position.y, 1);
  ASSERT_EQ(position.z, 2);
}

TEST(CHUNK, addCube_getCube) {
  auto chunk = Chunk(0,0,0);
  auto cube = Cube();
  cube.blockType() = 0;

  cube.position() = glm::vec3(3,2,1);
  cube.selected() = 0;
  chunk.addCube(cube, cube.position().x, cube.position().y, cube.position().z);
  auto got = chunk.getCube(cube.position().x, cube.position().y,
cube.position().z); ASSERT_EQ(cube.position(), got->position());
  ASSERT_EQ(cube.blockType(), got->blockType());
  ASSERT_EQ(cube.selected(), got->selected());
}

TEST(CHUNK, mesh) {
  auto chunk = Chunk(0, 0, 0);
  auto cube = Cube();
  cube.blockType() = 0;
  cube.position() = glm::vec3(3, 2, 1);
  cube.selected() = 0;
  chunk.addCube(cube, cube.position().x, cube.position().y, cube.position().z);

  auto mesh = chunk.mesh();
  ASSERT_EQ(mesh->positions.size(), 36);
  ASSERT_EQ(mesh->blockTypes.size(), 36);
  ASSERT_EQ(mesh->selects.size(), 36);

  cube = Cube();
  cube.blockType() = 0;
  cube.position() = glm::vec3(1, 2, 3);
  cube.selected() = 0;
  chunk.addCube(cube, cube.position().x, cube.position().y, cube.position().z);

  mesh = chunk.mesh();
  ASSERT_EQ(mesh->positions.size(), 36 * 2);
  ASSERT_EQ(mesh->blockTypes.size(), 36 * 2);
  ASSERT_EQ(mesh->selects.size(), 36 * 2);
}

TEST(CHUNK, meshEmptyChunk)
{
  auto chunk = Chunk(0, 0, 0);
  auto cube = Cube();
  cube.blockType() = 0;
  cube.position() = glm::vec3(3, 2, 1);
  cube.selected() = 0;

  auto mesh = chunk.mesh();
  ASSERT_EQ(mesh->positions.size(), 0);
  ASSERT_EQ(mesh->blockTypes.size(), 0);
  ASSERT_EQ(mesh->selects.size(), 0);
}
