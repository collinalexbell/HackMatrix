#pragma once

#include <imgui.h>
#include <string>
#include <optional>
#include <memory>
#include "./Client.h"
#include "./Server.h"

class Engine;
namespace MultiPlayer {

class Gui {
public:
    Gui(Engine*);
    ~Gui();

    void Render();

    bool IsConnectButtonClicked() const;
    const std::string& GetAddress() const;
    int GetPort() const;

private:
    bool connect;
    std::string address;
    int port;
    Engine *engine;
    std::shared_ptr<MultiPlayer::Client> client;
    std::shared_ptr<MultiPlayer::Server> server;
};

}
