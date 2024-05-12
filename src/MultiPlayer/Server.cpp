#include "MultiPlayer/Server.h"
#include <iostream>
#include <algorithm>
#include "glm/glm.hpp"
#include "systems/Player.h"

namespace MultiPlayer {

Server::Server() : server(nullptr), isRunning(false) {
  if (enet_initialize() != 0) {
    std::cout << "Failed to initialize enet." << std::endl;
  }
}

Server::~Server() {
  Stop();
  enet_deinitialize();
}

bool Server::Start(int port) {
  ENetAddress address;
  address.host = ENET_HOST_ANY;
  address.port = port;

  server = enet_host_create(&address, 32, 10, 0, 0);
  if (server == NULL) {
    return false;
  }
  isRunning = true;
  return true;
}

void Server::Poll(std::shared_ptr<EntityRegistry> registry) {
  ENetEvent event;
  while (enet_host_service(server, &event, 0) > 0) {
    switch (event.type) {
      case ENET_EVENT_TYPE_CONNECT:
        {
          uint32_t playerId = event.peer->connectID;
          clients.push_back(playerId);
          ENetPacket* packet = enet_packet_create(NULL, sizeof(uint32_t) , ENET_PACKET_FLAG_RELIABLE);
          memcpy(packet->data, &playerId, sizeof(uint32_t));
          enet_host_broadcast(server, 2, packet);
        }
        break;
      case ENET_EVENT_TYPE_RECEIVE:
        // Handle received data
        if (event.channelID == 1) {  // Player data channel
          glm::vec3* data = reinterpret_cast<glm::vec3*>(event.packet->data);
          glm::vec3 position = data[0];
          glm::vec3 front = data[1];
          uint32_t playerID = event.peer->connectID;

          ENetPacket* packet = enet_packet_create(NULL, sizeof(glm::vec3) * 2 + sizeof(uint32_t) , ENET_PACKET_FLAG_UNSEQUENCED);

          memcpy(packet->data, &position, sizeof(glm::vec3));
          memcpy(static_cast<unsigned char*>(packet->data) + sizeof(glm::vec3), &front, sizeof(glm::vec3));
          memcpy(static_cast<unsigned char*>(packet->data) + sizeof(glm::vec3) * 2, &playerID, sizeof(uint32_t));


          enet_host_broadcast(server, 1, packet);
        }

        enet_packet_destroy(event.packet);
        break;
      case ENET_EVENT_TYPE_DISCONNECT:
        {
          auto toRemove = std::find(clients.begin(), clients.end(), event.peer->connectID);
          if(toRemove != clients.end()){
            clients.erase(toRemove);
          }
          break;
        }
      default:
        break;
    }
  }
}

void Server::Stop() {
  clients.clear();
  if (server != nullptr) {
    enet_host_destroy(server);
    server = nullptr;
    isRunning = false;
  }
}

bool Server::IsRunning() {
  return isRunning;
}

const std::vector<uint32_t> Server::GetClients() {
  return clients;
}

}
