#pragma once

#include <enet/enet.h>
#include <vector>

namespace MultiPlayer {


class Server {
public:
    Server();
    ~Server();

    bool Start(int port);
    void Stop();
    void Poll();
    bool IsRunning();
    const std::vector<ENetPeer*> GetClients();

private:
    ENetHost* server;
    std::vector<ENetPeer*> clients;
    bool isRunning;
};

}
