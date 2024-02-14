#include "blocks.h"
#include "catch_amalgamated.hpp"
#include <zmq/zmq.hpp>

#include "api.h"

TEST_CASE("true") {
  //World* world = mockWorld();
  auto context = zmq::context_t(2);
  auto socket = zmq::socket_t(context, zmq::socket_type::req);
  REQUIRE(true);
}
