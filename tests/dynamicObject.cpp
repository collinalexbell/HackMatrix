#include "dynamicObject.h"
#include <gtest/gtest.h>
#include <glm/glm.hpp>
#include <memory>

TEST(DynamicObjectSpace, returnsOnlyLiveObjectIds)
{
  auto space = DynamicObjectSpace();
  auto cubeA =
    make_shared<DynamicCube>(glm::vec3(0, 0, 0), glm::vec3(1, 1, 1), glm::vec3(1, 0, 0));
  auto cubeB =
    make_shared<DynamicCube>(glm::vec3(1, 0, 0), glm::vec3(1, 1, 1), glm::vec3(0, 1, 0));

  space.addObject(cubeA);
  space.addObject(cubeB);
  space.queueRemoveObjectById(cubeA->id());

  auto idsBeforeFlush = space.getObjectIds();
  ASSERT_EQ(idsBeforeFlush.size(), 1);
  EXPECT_EQ(idsBeforeFlush[0], cubeB->id());

  space.flushQueuedRemovals();

  auto idsAfterFlush = space.getObjectIds();
  ASSERT_EQ(idsAfterFlush.size(), 1);
  EXPECT_EQ(idsAfterFlush[0], cubeB->id());
  EXPECT_EQ(space.getObjectById(cubeA->id()), nullptr);
}

TEST(DynamicObjectSpace, removesObjectsInBoundingBox)
{
  auto space = DynamicObjectSpace();
  auto cubeA =
    make_shared<DynamicCube>(glm::vec3(0, 0, 0), glm::vec3(1, 1, 1), glm::vec3(1, 0, 0));
  auto cubeB =
    make_shared<DynamicCube>(glm::vec3(5, 5, 5), glm::vec3(1, 1, 1), glm::vec3(0, 1, 0));

  space.addObject(cubeA);
  space.addObject(cubeB);
  space.queueRemoveObjectsInBox(glm::vec3(-1, -1, -1), glm::vec3(1, 1, 1));
  space.flushQueuedRemovals();

  auto ids = space.getObjectIds();
  ASSERT_EQ(ids.size(), 1);
  EXPECT_EQ(ids[0], cubeB->id());
}
