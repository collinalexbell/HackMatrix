#pragma once

#include <enet/enet.h>

namespace MultiPlayer {

class Server {
public:
    Server();
    ~Server();

    bool Start(int port);
    void Stop();

private:
    ENetHost* server;
};

}
