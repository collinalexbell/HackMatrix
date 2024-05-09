#include "MultiPlayer/Gui.h"

namespace MultiPlayer {

Gui::Gui() : connect(false), address("127.0.0.1"), port(7777) {}

Gui::~Gui() {}

void Gui::Render() {
  const int cAddressSize = 1024;
  char cAddress[cAddressSize];
  ImGui::InputText("Address", cAddress, cAddressSize);
  address = cAddress;
  ImGui::InputInt("Port", &port);
  if (ImGui::Button("Connect")) {
    connect = true;
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
