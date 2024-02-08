#include "catch_amalgamated.hpp"
#include "dynamicObject.h"
#include <glm/glm.hpp>
#include <memory>

TEST_CASE("DynamicObjectSpace.getObjectById() returns object") {
  auto space = DynamicObjectSpace();
  auto cube = make_shared<DynamicCube>(glm::vec3(0,0,0), glm::vec3(0,0,0));
  space.addObject(cube);
  REQUIRE(space.getObjectById(cube->id()) == cube);
}

TEST_CASE("DynamicObjectSpace.getObjectIds() returns list of object ids") {
  auto space = DynamicObjectSpace();
  shared_ptr<DynamicObject> cubes[2] = {
    make_shared<DynamicCube>(glm::vec3(0, 0, 0), glm::vec3(0, 0, 0)),
    make_shared<DynamicCube>(glm::vec3(0, 0, 0), glm::vec3(0, 0, 0))};
  for(auto cube: cubes) {
    space.addObject(cube);
  }
  for (auto cube : cubes) {
    vector<int> ids = space.getObjectIds();
    REQUIRE(find(ids.begin(), ids.end(), cube->id()) != ids.end());
  }
}
