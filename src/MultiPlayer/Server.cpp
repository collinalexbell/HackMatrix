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

  server = enet_host_create(&address, 32, 2, 0, 0);
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
        clients.push_back(event.peer);
        // You can perform additional actions here, such as sending a welcome message
        // or updating your GUI to display the new connection
        break;
      case ENET_EVENT_TYPE_RECEIVE:
        // Handle received data
        if (event.channelID == 1) {  // Player data channel
          glm::vec3* data = reinterpret_cast<glm::vec3*>(event.packet->data);
          glm::vec3 position = data[0];
          glm::vec3 front = data[1];

          uint32_t playerID = event.peer->connectID;
          systems::movePlayer(registry, playerID, position, front, 1.0 / 20.0);

        }

        enet_packet_destroy(event.packet);
        break;
      case ENET_EVENT_TYPE_DISCONNECT:
        {
          auto toRemove = std::find(clients.begin(), clients.end(), event.peer);
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

const std::vector<ENetPeer*> Server::GetClients() {
  return clients;
}

}
