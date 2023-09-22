#include <zmq/zmq.hpp>
#include "api.h"
#include <iostream>

Api::Api(std::string bindAddress) {
  initZmq(bindAddress);
}


void Api::initZmq(std::string bindAddress) {
  context = zmq::context_t(2);
  socket = zmq::socket_t(context, zmq::socket_type::rep);
  socket.bind (bindAddress);
}

void Api::pollFor(World* world) {
  zmq::message_t request;

  zmq::recv_result_t result = socket.recv(request, zmq::recv_flags::dontwait);
  if(result >= 0) {
    std::string data(static_cast<char*>(request.data()), request.size());
    std::cout  << data << std::endl;

    std::istringstream iss(data);

    std::string command;
    int x,y,z,type;
    iss >> command >> x >> y >> z >> type;

    if(command == "c") {
      world->addCube(glm::vec3(x,y,z), type);
    }

    //  Send reply back to client
    zmq::message_t reply (5);
    memcpy (reply.data (), "recv", 5);
    socket.send (reply, zmq::send_flags::none);
  }
}
