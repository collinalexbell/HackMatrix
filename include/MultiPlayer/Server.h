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
    const std::vector<uint32_t> GetClients();

private:
    ENetHost* server;
    std::vector<uint32_t> clients;
    bool isRunning;
};

}
