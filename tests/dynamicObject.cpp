#include "fakeit.hpp"
#include "dynamicObject.h"
#include <glm/glm.hpp>
#include <memory>

TEST(DynamicObjectSpace_getObjectById, returnsObject) {
  auto space = DynamicObjectSpace();
  auto cube = make_shared<DynamicCube>(glm::vec3(0,0,0), glm::vec3(0,0,0));
  space.addObject(cube);
  ASSERT_TRUE(space.getObjectById(cube->id()) == cube);
}

TEST(DynamicObjectSpace_getObjectIds, returnsIds) {
  auto space = DynamicObjectSpace();
  shared_ptr<DynamicObject> cubes[2] = {
    make_shared<DynamicCube>(glm::vec3(0, 0, 0), glm::vec3(0, 0, 0)),
    make_shared<DynamicCube>(glm::vec3(0, 0, 0), glm::vec3(0, 0, 0))};
  for(auto cube: cubes) {
    space.addObject(cube);
  }
  for (auto cube : cubes) {
    vector<int> ids = space.getObjectIds();
    ASSERT_TRUE(find(ids.begin(), ids.end(), cube->id()) != ids.end());
  }
}
