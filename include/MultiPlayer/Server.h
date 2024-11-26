#pragma once

#include <enet/enet.h>
#include <thread>
#include <atomic>
#include <glm/glm.hpp>

namespace MultiPlayer {

struct PlayerUpdate
{
  uint32_t playerID;
  glm::vec3 position;
  glm::vec3 front;
};

enum CHANNEL_TYPE
{
  PLAYER_UPDATE = 1
};

class Server
{
public:
  Server();
  ~Server();

  bool Start(int port);
  void Stop();
  void PollLoop();
  bool IsRunning();

private:
  ENetHost* server;
  std::atomic<bool> isRunning;
  std::thread pollThread;
  PlayerUpdate getPlayerUpdateFromEvent(ENetEvent&);
  void broadcastPlayerUpdate(PlayerUpdate);
};

}
