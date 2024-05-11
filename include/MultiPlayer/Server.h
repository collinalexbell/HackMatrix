#pragma once

#include <enet/enet.h>
#include <vector>
#include "entity.h"

namespace MultiPlayer {


class Server {
public:
    Server();
    ~Server();

    bool Start(int port);
    void Stop();
    void Poll(std::shared_ptr<EntityRegistry>);
    bool IsRunning();
    const std::vector<ENetPeer*> GetClients();

private:
    ENetHost* server;
    std::vector<ENetPeer*> clients;
    bool isRunning;
};

}
