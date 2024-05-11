#pragma once

#include <imgui.h>
#include <string>
#include <optional>
#include <memory>
#include "./Client.h"
#include "./Server.h"

namespace MultiPlayer {

class Gui {
public:
    Gui(std::optional<std::shared_ptr<MultiPlayer::Client>>& client,
        std::optional<std::shared_ptr<MultiPlayer::Server>>& server);
    ~Gui();

    void Render();

    bool IsConnectButtonClicked() const;
    const std::string& GetAddress() const;
    int GetPort() const;

private:
    bool connect;
    std::string address;
    int port;
    std::optional<std::shared_ptr<MultiPlayer::Client>>& client;
    std::optional<std::shared_ptr<MultiPlayer::Server>>& server;
};

}
