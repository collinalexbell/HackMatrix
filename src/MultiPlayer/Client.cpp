#include "MultiPlayer/Client.h"
#include <iostream>
#include <glm/glm.hpp>
#include "camera.h"
#include <GLFW/glfw3.h>
#include "systems/Player.h"

namespace MultiPlayer {

  Client::Client(shared_ptr<EntityRegistry> registry):
    registry(registry), client(nullptr), peer(nullptr) {
    if (enet_initialize() != 0) {
      std::cout << "Failed to initialize enet." << std::endl;
    }
  }

  Client::~Client() {
    disconnect();
    enet_deinitialize();
  }

  bool Client::connect(const std::string& address, int port) {
    if(peer != nullptr) {
      enet_peer_reset(peer);
      peer = nullptr;
    }
    if(client != nullptr) {
      enet_host_destroy(client);
      client = nullptr;

    }
    client = enet_host_create(NULL, 1, 10, 0, 0);
    if (client == NULL) {
      std::cout << "Failed to create client." << std::endl;
      return false;
    }

    ENetAddress enetAddress;
    enet_address_set_host(&enetAddress, address.c_str());
    enetAddress.port = port;

    peer = enet_host_connect(client, &enetAddress, 10, 0);
    if (peer == NULL) {
      std::cout << "Failed to connect to server." << std::endl;
      enet_host_destroy(client);
      client = nullptr;
      return false;
    }

    ENetEvent event;
    if (enet_host_service(client, &event, 5000) > 0 && event.type == ENET_EVENT_TYPE_CONNECT) {
      std::cout << "Connected to server." << std::endl;
      enet_host_flush(client);
      _isConnected = true;
      return true;
    } else {
      std::cout << "didn't get response" << std::endl;
      enet_peer_reset(peer);
      peer = nullptr;
      enet_host_destroy(client);
      client = nullptr;
      return false;
    }
  }

  void Client::poll() {
    if(isConnected()){
      ENetEvent event;
      while (enet_host_service(client, &event, 0) > 0) {
        switch(event.type) {
          case ENET_EVENT_TYPE_RECEIVE:
            if (event.channelID == 1) {  // Player data channel
                                         // Get the packet data
              unsigned const char* packetData = static_cast<unsigned const char*>(event.packet->data);

              // Extract position from the packet
              glm::vec3 position;
              memcpy(&position, packetData, sizeof(glm::vec3));

              // Extract front from the packet
              glm::vec3 front;
              memcpy(&front, packetData + sizeof(glm::vec3), sizeof(glm::vec3));

              // Extract playerID from the packet
              uint32_t playerID;
              memcpy(&playerID, packetData + sizeof(glm::vec3) * 2, sizeof(uint32_t));

              if(peer->connectID != playerID) {
                systems::movePlayer(registry, playerID, position, front, 1.0 / 20.0);
              }
            }
            if(event.channelID == 2) {
              auto packetData = static_cast<unsigned const char*>(event.packet->data);
              uint32_t playerID;
              memcpy(&playerID, packetData, sizeof(uint32_t));
              if(peer->connectID != playerID) {
                systems::registerPlayer(registry, playerID);
              }
            }
            enet_packet_destroy(event.packet);
            break;
          default:
            break;
        }
      }
    }
  }

  void Client::disconnect() {
    if (peer != nullptr) {
      enet_peer_disconnect(peer, 0);
      ENetEvent event;
      while (enet_host_service(client, &event, 3000) > 0) {
        switch (event.type) {
          case ENET_EVENT_TYPE_RECEIVE:
            enet_packet_destroy(event.packet);
            break;
          case ENET_EVENT_TYPE_DISCONNECT:
            std::cout << "Disconnected from server." << std::endl;
            _isConnected = false;
            break;
        }
      }
      enet_peer_reset(peer);
      peer = nullptr;
    }
    if (client != nullptr) {
      enet_host_destroy(client);
      client = nullptr;
    }
  }

  bool Client::isConnected() {
    return _isConnected;
  }


  bool Client::sendPlayer(glm::vec3 position, glm::vec3 front) {
    if (_isConnected) {

      ENetPacket* packet = enet_packet_create(NULL, sizeof(glm::vec3) * 2, ENET_PACKET_FLAG_UNSEQUENCED);

      // Copy the player's position and front vector into the packet data
      glm::vec3* data = reinterpret_cast<glm::vec3*>(packet->data);
      data[0] = position;
      data[1] = front;

      // Send the packet on the dedicated channel for players
      enet_peer_send(peer, 1, packet);
      enet_host_flush(client);

      return true;
    }

    return false;
  }
}
