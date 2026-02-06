#pragma once

#include "protos/api.pb.h"

#include <atomic>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

struct MenuBootableSummary
{
  std::string name;
  std::string cmd;
  std::string args;
  int width = 0;
  int height = 0;
  std::optional<std::pair<int, int>> xy;
  bool bootOnStartup = false;
  bool killOnExit = false;
  bool transparent = false;
};

struct MenuPositionSummary
{
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
  float scale = 1.0f;
};

struct MenuEntitySummary
{
  int entityId = 0;
  int rawId = 0;
  std::optional<std::string> modelPath;
  std::optional<MenuBootableSummary> bootable;
  std::optional<MenuPositionSummary> position;
  bool hasLight = false;
};

struct MenuSnapshot
{
  double fps = 0.0;
  EngineStatus status;
  std::vector<std::string> logs;
  std::vector<MenuEntitySummary> entities;
};

class MenuWebServer
{
public:
  explicit MenuWebServer(int port = 8675, std::string host = "127.0.0.1");
  ~MenuWebServer();

  void start();
  void stop();
  void publish(MenuSnapshot snapshot);

private:
  std::atomic<bool> running{ false };
  int serverFd = -1;
  int port = 8675;
  std::string host = "127.0.0.1";
  std::thread serverThread;
  std::mutex snapshotMutex;
  MenuSnapshot latest;

  void serve();
  void handleClient(int clientFd);
  std::string serializeSnapshot();
  std::string renderHtmlPage();
};
