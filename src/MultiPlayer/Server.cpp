#include "MultiPlayer/Server.h"
#include <iostream>

namespace MultiPlayer {

Server::Server() : server(nullptr) {
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
        std::cout << "Failed to create server." << std::endl;
        return false;
    }

    std::cout << "Server started on port " << port << std::endl;
    return true;
}

void Server::Stop() {
    if (server != nullptr) {
        enet_host_destroy(server);
        server = nullptr;
    }
}

}
