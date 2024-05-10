#pragma once

#include <enet/enet.h>
#include <string>

namespace MultiPlayer {

class Client {
public:
    Client();
    ~Client();

    bool Connect(const std::string& address, int port);
    void Disconnect();

private:
    ENetHost* client;
    ENetPeer* peer;
};

}
