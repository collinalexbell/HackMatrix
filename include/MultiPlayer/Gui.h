#pragma once

#include <imgui.h>
#include <string>

namespace MultiPlayer {

class Gui {
public:
    Gui();
    ~Gui();

    void Render();

    bool IsConnectButtonClicked() const;
    const std::string& GetAddress() const;
    int GetPort() const;

private:
    bool connect;
    std::string address;
    int port;
};

}
