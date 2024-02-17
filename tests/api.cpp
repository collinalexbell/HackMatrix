#include "fakeit.hpp"

#include "api.h"
#include "blocks.h"
#include "dynamicObject.h"
#include "worldInterface.h"
#include <algorithm>
#include <gtest/gtest.h>
#include <zmq/zmq.hpp>
#undef Status
#include "protos/api.pb.h"

using namespace fakeit;

TEST(API, move) {
}

TEST(API, getIds) {
  auto space = make_shared<DynamicObjectSpace>();
  space->addObject(make_shared<DynamicCube>(glm::vec3(0,0,0), glm::vec3(0,0,0)));
  auto expectedIds = space->getObjectIds();
  Mock<WorldInterface> world;
  When(Method(world, getDynamicObjects)).AlwaysReturn(space);
  Api api("tcp://*:1234", &world.get());
  auto context = zmq::context_t(2);
  auto socket = zmq::socket_t(context, zmq::socket_type::req);
  socket.connect("tcp://localhost:1234");
  ApiRequest request;
  request.set_type(MessageType::GET_IDS);
  request.mutable_getids();

  // Serialize the message to a string
  std::string serializedMessage;
  if (!request.SerializeToString(&serializedMessage)) {
    throw "failed to serialize message";
  }

  // Create a ZMQ message from the serialized string
  zmq::message_t zmqMessage(serializedMessage.size());
  memcpy(zmqMessage.data(), serializedMessage.data(), serializedMessage.size());

  // Send the message over ZMQ
  socket.send(zmqMessage, zmq::send_flags::none);
  // Optionally, receive a reply if expected
  zmq::message_t reply;
  auto result = socket.recv(reply, zmq::recv_flags::none);
  ObjectIds ids;

  if (result) {
    std::string replyContent(static_cast<char *>(reply.data()), reply.size());
    if (ids.ParseFromArray(reply.data(), reply.size())) {
      ASSERT_EQ(ids.ids().size(), expectedIds.size());
      for (int i = 0; i < ids.ids().size(); i++) {
        auto id = ids.ids().data()[i];
        auto result = find(expectedIds.begin(), expectedIds.end(), id);
        ASSERT_NE(result, expectedIds.end());
      }
    } else {
      throw "couldn't parse into ObjectIds";
    }
  } else {
    throw "should have result";
  }
}
