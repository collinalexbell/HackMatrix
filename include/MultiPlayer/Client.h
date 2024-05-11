#pragma once

#include <enet/enet.h>
#include <string>
#include <glm/glm.hpp>
#include "entity.h"

namespace MultiPlayer {

class Client {
public:
    Client(std::shared_ptr<EntityRegistry>);
    ~Client();

    bool Connect(const std::string& address, int port);
    bool IsConnected();
    void Disconnect();
    bool SendPlayer(glm::vec3, glm::vec3);
    void StartUpdateThread();

private:
    ENetHost* client;
    ENetPeer* peer;
    bool isConnected = false;
    double lastUpdate = 0;
    double UPDATE_EVERY = 1.0/20.0;
    std::shared_ptr<EntityRegistry> registry;
};

}
