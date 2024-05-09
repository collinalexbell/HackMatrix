#pragma once

#include <imgui.h>
#include <string>

namespace MultiPlayer {

class Gui {
public:
    Gui();
    ~Gui();

    void Run();

private:
    bool connect;
    std::string address;
    int port;
};

}
