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
          // Handle received data
          if (event.channelID == 1) { // Player data channel
            glm::vec3* data = reinterpret_cast<glm::vec3*>(event.packet->data);
            glm::vec3 position = data[0];
            glm::vec3 front = data[1];
            uint32_t playerID = event.peer->connectID;

            ENetPacket* packet =
              enet_packet_create(NULL,
                                 sizeof(glm::vec3) * 2 + sizeof(uint32_t),
                                 ENET_PACKET_FLAG_UNSEQUENCED);

            memcpy(packet->data, &position, sizeof(glm::vec3));
            memcpy(static_cast<unsigned char*>(packet->data) +
                     sizeof(glm::vec3),
                   &front,
                   sizeof(glm::vec3));
            memcpy(static_cast<unsigned char*>(packet->data) +
                     sizeof(glm::vec3) * 2,
                   &playerID,
                   sizeof(uint32_t));

            enet_host_broadcast(server, 1, packet);
            enet_host_flush(server);
          }

          enet_packet_destroy(event.packet);
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
