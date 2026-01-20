#include "MultiPlayer/Server.h"
#include <iostream>
#include <glm/glm.hpp>
#include <vector>
#include <unistd.h>

namespace MultiPlayer {

Server::Server()
  : server(nullptr)
  , isRunning(false)
{
  if (enet_initialize() != 0) {
    std::cout << "Failed to initialize enet." << std::endl;
  }
}

Server::~Server()
{
  Stop();
  enet_deinitialize();
}

bool
Server::Start(int port)
{
  ENetAddress address;
  address.host = ENET_HOST_ANY;
  address.port = port;

  server = enet_host_create(&address, 32, 10, 0, 0);
  if (server == NULL) {
    return false;
  }
  isRunning = true;
  enet_host_flush(server);

  pollThread = std::thread([this]() { PollLoop(); });
  pollThread.detach();

  return true;
}

PlayerUpdate
Server::getPlayerUpdateFromEvent(ENetEvent& event)
{
  PlayerUpdate update;
  glm::vec3* data = reinterpret_cast<glm::vec3*>(event.packet->data);
  update.position = data[0];
  update.front = data[1];
  update.playerID = event.peer->connectID;
  return update;
}

void
Server::broadcastPlayerUpdate(PlayerUpdate update)
{
  ENetPacket* packet =
    enet_packet_create(NULL,
                       sizeof(glm::vec3) * 2 + sizeof(uint32_t),
                       ENET_PACKET_FLAG_UNSEQUENCED);

  memcpy(packet->data, &update.position, sizeof(glm::vec3));
  memcpy(static_cast<unsigned char*>(packet->data) + sizeof(glm::vec3),
         &update.front,
         sizeof(glm::vec3));
  memcpy(static_cast<unsigned char*>(packet->data) + sizeof(glm::vec3) * 2,
         &update.playerID,
         sizeof(uint32_t));

  enet_host_broadcast(server, 1, packet);
  enet_host_flush(server);
}

void
Server::PollLoop()
{
  std::vector<uint32_t> clients;
  ENetEvent event;
  while (isRunning) {
    if (enet_host_service(server, &event, 0) > 0) {
      switch (event.type) {
        case ENET_EVENT_TYPE_CONNECT: {
          enet_host_flush(server);
          uint32_t playerId = event.peer->connectID;
          ENetPacket* packet = enet_packet_create(
            &playerId, sizeof(uint32_t), ENET_PACKET_FLAG_RELIABLE);
          enet_host_broadcast(server, 2, packet);
          enet_host_flush(server);

          for (auto client : clients) {
            ENetPacket* packet = enet_packet_create(
              &client, sizeof(uint32_t), ENET_PACKET_FLAG_RELIABLE);
            enet_peer_send(event.peer, 2, packet);
          }
          enet_host_flush(server);
          clients.push_back(playerId);
        } break;
        case ENET_EVENT_TYPE_RECEIVE:
          if (event.channelID == PLAYER_UPDATE) {
            auto update = getPlayerUpdateFromEvent(event);
            broadcastPlayerUpdate(update);
          }
          break;
        case ENET_EVENT_TYPE_DISCONNECT: {
          break;
        }
        default:
          break;
      }
    } else {
      usleep(10000);
    }
  }
}

void
Server::Stop()
{
  isRunning = false;
  if (server != nullptr) {
    enet_host_destroy(server);
    server = nullptr;
  }
}

bool
Server::IsRunning()
{
  return isRunning;
}
}
