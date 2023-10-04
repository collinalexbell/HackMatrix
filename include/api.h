#ifndef __API_H__
#define __API_H__
#include <zmq/zmq.hpp>
#include <string>
#include "world.h"

using namespace std;

class CommandServer {
  zmq::context_t context;
  zmq::socket_t socket;
public:
  void pollForWorldCommands(World* world);
  CommandServer(std::string bindAddress);
};

class Api {
  CommandServer* commandServer;
 public:
  Api(std::string bindAddress);
  void pollFor(World* world);
  void requestWorldData(World*, string);
};

#endif
