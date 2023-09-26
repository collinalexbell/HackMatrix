#include <zmq/zmq.hpp>
#include "api.h"
#include <iostream>

using namespace std;

Api::Api(std::string bindAddress) {
  initZmq(bindAddress);
}


void Api::initZmq(std::string bindAddress) {
  context = zmq::context_t(2);
  socket = zmq::socket_t(context, zmq::socket_type::rep);
  socket.bind (bindAddress);
}

void Api::initWorld(World* world, string serverAddr) {
  zmq::message_t reply;
  zmq::message_t request(5);
  zmq::context_t clientContext = zmq::context_t(2);
  zmq::socket_t clientSocket = zmq::socket_t(clientContext, zmq::socket_type::req);
  try {
    clientSocket.connect(serverAddr);
    memcpy (request.data (), "init", 5);
    clientSocket.send(request, zmq::send_flags::none);
    zmq::recv_result_t result = clientSocket.recv(reply, zmq::recv_flags::dontwait);
  } catch(...) {
    cout << "couldn't connect to the init server" << endl;
  }
}

void Api::pollFor(World* world) {
  zmq::message_t request;

  zmq::recv_result_t result = socket.recv(request, zmq::recv_flags::dontwait);
  if(result >= 0) {
    std::string data(static_cast<char*>(request.data()), request.size());
    std::cout  << data << std::endl;

    std::istringstream iss(data);

    while(!iss.eof()) {
      std::string command;
      int type;
      int x,y,z;
      iss >> command >> x >> y >> z >> type;

      if(command == "c") {
        world->addCube(x,y,z,type);
      }
    }

    //  Send reply back to client
    zmq::message_t reply (5);
    memcpy (reply.data (), "recv", 5);
    socket.send (reply, zmq::send_flags::none);
  }
}
