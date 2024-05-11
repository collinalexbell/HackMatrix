#pragma once

#include <enet/enet.h>
#include <string>
#include <glm/glm.hpp>

namespace MultiPlayer {

class Client {
public:
    Client();
    ~Client();

    bool Connect(const std::string& address, int port);
    bool IsConnected();
    void Disconnect();
    bool SendPlayer(glm::vec3, glm::vec3);

private:
    ENetHost* client;
    ENetPeer* peer;
    bool isConnected = false;
};

}
