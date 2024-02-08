#include "catch_amalgamated.hpp"
#include "dynamicObject.h"
#include <glm/glm.hpp>

TEST_CASE("getObjectById() returns object", "[DynamicSpace]") {
  auto space = DynamicObjectSpace();
  auto cube = make_shared<DynamicCube>(glm::vec3(0,0,0), glm::vec3(0,0,0));
  space.addObject(cube);
  REQUIRE(space.getObjectById(cube->id()) == cube);
}
