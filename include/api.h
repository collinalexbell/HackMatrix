#ifndef __API_H__
#define __API_H__
#include <zmq/zmq.hpp>
#include <string>
#include "world.h"

using namespace std;

class CommandServer {
  zmq::socket_t socket;
public:
  void pollForWorldCommands(World* world);
  CommandServer(std::string bindAddress, zmq::context_t& context);
};

class Api {
  zmq::context_t context;
  CommandServer* commandServer;
 public:
  Api(std::string bindAddress);
  ~Api();
  void pollFor(World* world);
  void requestWorldData(World*, string);
};

#endif
