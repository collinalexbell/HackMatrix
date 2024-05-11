#include "MultiPlayer/Client.h"
#include <iostream>

namespace MultiPlayer {

Client::Client() : client(nullptr), peer(nullptr) {
    if (enet_initialize() != 0) {
        std::cout << "Failed to initialize enet." << std::endl;
    }
}

Client::~Client() {
    Disconnect();
    enet_deinitialize();
}

bool Client::Connect(const std::string& address, int port) {
    client = enet_host_create(NULL, 1, 2, 0, 0);
    if (client == NULL) {
        std::cout << "Failed to create client." << std::endl;
        return false;
    }

    ENetAddress enetAddress;
    enet_address_set_host(&enetAddress, address.c_str());
    enetAddress.port = port;

    peer = enet_host_connect(client, &enetAddress, 2, 0);
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

void Client::Disconnect() {
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

}
