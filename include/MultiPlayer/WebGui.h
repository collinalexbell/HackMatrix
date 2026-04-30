#pragma once

#include <imgui.h>
#include <memory>
#include <optional>
#include <string>

#ifdef True
#undef True
#endif

#ifdef False
#undef False
#endif

#include <crow.h>

#include "./Client.h"
#include "./Server.h"
#include <thread>

class Engine;
namespace MultiPlayer {

class WebGui
{
public:
  WebGui(Engine*);
  ~WebGui();

  void Render();

private:
  std::thread serverThread;
  crow::SimpleApp app;
  bool connect;
  std::string address;
  int port;
  Engine* engine;
  std::shared_ptr<MultiPlayer::Client> client;
  std::shared_ptr<MultiPlayer::Server> server;
};

}
