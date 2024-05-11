#include "MultiPlayer/Server.h"
#include <iostream>
#include <algorithm>

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

void Server::Poll() {
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
