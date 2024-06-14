#pragma once

#include <enet/enet.h>
#include <thread>
#include <atomic>

namespace MultiPlayer {


class Server {
public:
    Server();
    ~Server();

    bool Start(int port);
    void Stop();
    void Poll();
    bool IsRunning();

private:
    ENetHost* server;
    std::atomic<bool> isRunning;
    std::thread pollThread;

};

}
