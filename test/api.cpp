#include "fakeit.hpp"

#include "api.h"
#include "blocks.h"
#include "dynamicObject.h"
#include "worldInterface.h"
#include <zmq/zmq.hpp>


using namespace fakeit;

TEST_CASE("true") {
  //World* world = mockWorld();
  auto space = make_shared<DynamicObjectSpace>();
  space->addObject(make_shared<DynamicCube>(glm::vec3(0,0,0), glm::vec3(0,0,0)));
  fakeit::Mock<WorldInterface> world;
  When(Method(world, getDynamicObjects)).AlwaysReturn(space);
  Api api("tcp://*:1234", &world.get());
  auto context = zmq::context_t(2);
  auto socket = zmq::socket_t(context, zmq::socket_type::req);
  socket.connect("tcp://localhost:1234");
  //REQUIRE(true);
}
