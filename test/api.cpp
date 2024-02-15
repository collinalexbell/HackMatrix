#include "blocks.h"
#include "catch_amalgamated.hpp"
#include "fakeit.hpp"
#include <zmq/zmq.hpp>

#include "api.h"
#include "worldInterface.h"

TEST_CASE("true") {
  //World* world = mockWorld();
  fakeit::Mock<WorldInterface> world;
  Api api("tcp://*:1234", &world.get());
  auto context = zmq::context_t(2);
  auto socket = zmq::socket_t(context, zmq::socket_type::req);
  REQUIRE(true);
}
