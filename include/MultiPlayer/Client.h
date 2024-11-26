#pragma once

#include <enet/enet.h>
#include <string>
#include <glm/glm.hpp>
#include "entity.h"

namespace MultiPlayer {

class Client
{
public:
  Client(std::shared_ptr<EntityRegistry>);
  ~Client();

  bool connect(const std::string& address, int port);
  bool isConnected();
  void disconnect();
  bool sendPlayer(glm::vec3, glm::vec3);
  void startUpdateThread();
  void updateThreadLoop();
  void poll();
  bool shouldSendPlayerPacket();
  void justSentPlayerPacket();

private:
  ENetHost* client;
  ENetPeer* peer;
  bool _isConnected = false;
  double lastUpdate = 0;
  double UPDATE_EVERY = 1.0 / 20.0;
  std::shared_ptr<EntityRegistry> registry;
};

}
