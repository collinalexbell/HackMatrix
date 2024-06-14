#define ENET_IMPLEMENTATION
#include "MultiPlayer/Server.h"

int main() {
  auto server = MultiPlayer::Server();
  server.Start(1234);
  while(server.IsRunning()) {
    server.Poll();
  }
}

