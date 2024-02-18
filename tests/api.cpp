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

zmq::message_t sendRequest(ApiRequest request, zmq::socket_t &socket) {
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
  if(!result) {
    throw "api did not return a valid result";
  }
  return reply;
}

auto testContext = zmq::context_t(2);

struct TestClientServer {
  unique_ptr<Api> server;
  unique_ptr<zmq::socket_t> client;
};

TestClientServer initClientServer(shared_ptr<Mock<WorldInterface>> world, unsigned int port) {
  stringstream serverString;
  stringstream clientString;
  serverString << "tcp://*:" << port;
  clientString << "tcp://localhost:" << port;
  TestClientServer rv = TestClientServer {
    make_unique<Api>(serverString.str(), &world->get()),
    make_unique<zmq::socket_t>(testContext, zmq::socket_type::req)
  };
  rv.client->connect(clientString.str());
  return rv;
}

TEST(API, move) {
  auto space = make_shared<DynamicObjectSpace>();
  auto obj = make_shared<DynamicCube>(glm::vec3(0, 0, 0), glm::vec3(0, 0, 0));
  space->addObject(obj);
  auto world = make_shared<Mock<WorldInterface>>();
  When(Method(*world, getDynamicObjects)).AlwaysReturn(space);
  TestClientServer clientSever = initClientServer(world, 1235);
  ApiRequest request;
  auto move = request.mutable_move();
  move->set_id(obj->id());
  move->set_xdelta(1.0);
  move->set_ydelta(1.0);
  move->set_zdelta(1.0);
  request.set_type(MessageType::MOVE);
  auto reply = sendRequest(request, *clientSever.client);
  ASSERT_LT(obj->getPosition().x - 1.0, 0.01);
  ASSERT_LT(obj->getPosition().y - 1.0, 0.01);
  ASSERT_LT(obj->getPosition().z - 1.0, 0.01);
}

TEST(API, getIds) {
  auto space = make_shared<DynamicObjectSpace>();
  space->addObject(make_shared<DynamicCube>(glm::vec3(0,0,0), glm::vec3(0,0,0)));
  auto expectedIds = space->getObjectIds();
  auto world = make_shared<Mock<WorldInterface>>();
  When(Method(*world, getDynamicObjects)).AlwaysReturn(space);
  auto clientServer = initClientServer(world, 1234);
  ApiRequest request;
  request.set_type(MessageType::GET_IDS);
  request.mutable_getids();

  auto reply = sendRequest(request, *clientServer.client);

  ObjectIds ids;

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
}
