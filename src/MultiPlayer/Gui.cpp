#include "MultiPlayer/Gui.h"
#include "MultiPlayer/Server.h"
#include "MultiPlayer/Client.h"
#include "imgui.h"
#include <enet/enet.h>

namespace MultiPlayer {

Gui::Gui(
    std::optional<std::shared_ptr<MultiPlayer::Client>>& client,
    std::optional<std::shared_ptr<MultiPlayer::Server>>& server
    ) : connect(false), address("127.0.0.1"), port(7777),
  client(client), server(server) {}

Gui::~Gui() {}

void Gui::Render() {
    const int cAddressSize = 1024;
    static char cAddress[cAddressSize];
    static int selectedMode = 0;
    const char* modes[] = { "Client", "Server" };

    ImGui::Combo("Server/Client", &selectedMode, modes, IM_ARRAYSIZE(modes));

    if (selectedMode == 0) {
        ImGui::InputText("Address", cAddress, cAddressSize);
        address = cAddress;
    }

    ImGui::InputInt("Port", &port);

    if (selectedMode == 0) {
      if(client.has_value() && client.value()->IsConnected()) {
        ImGui::Text("connected to server");
        if (ImGui::Button("Disconnect")) {
            client.value()->Disconnect();
            client = std::nullopt;
        }
      } else {
        if (ImGui::Button("Connect as Client")) {
          client = std::make_shared<Client>();
          client.value()->Connect(address, port);
        }
      }
    } else {
      if(server.has_value() && server.value()->IsRunning()) {
        ImGui::Text("clientCount: %d", (int)server.value()->GetClients().size());
        if (ImGui::Button("Stop Server")) {
          server.value()->Stop();
          server = std::nullopt;
        }
      } else {
        if (ImGui::Button("Host as Server")) {
            server = std::make_shared<Server>();
            server.value()->Start(port);
        }
      }
    }
}

bool Gui::IsConnectButtonClicked() const {
  return connect;
}

const std::string& Gui::GetAddress() const {
  return address;
}

int Gui::GetPort() const {
  return port;
}

}
