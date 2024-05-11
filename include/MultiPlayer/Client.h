#pragma once

#include <enet/enet.h>
#include <string>

namespace MultiPlayer {

class Client {
public:
    Client();
    ~Client();

    bool Connect(const std::string& address, int port);
    bool IsConnected();
    void Disconnect();

private:
    ENetHost* client;
    ENetPeer* peer;
    bool isConnected = false;
};

}
