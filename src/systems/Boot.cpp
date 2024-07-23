#include "systems/Boot.h"
#include "components/Bootable.h"
#include <optional>
#include <signal.h>

#include <string>
#include <iostream>
#include <fstream>
#include <thread>

bool
isShellScript(const std::string& filename)
{
  size_t len = filename.length();
  if (len >= 3 && filename.substr(len - 3) == ".sh") {
    return true;
  } else {
    return false;
  }
}

int
getShPID(char* pidfile)
{
  const int timeoutSeconds = 5;
  // Open the file for reading
  cout << "opening " << pidfile << endl;
  std::ifstream file(pidfile);
  int pid;

  auto start_time = std::chrono::steady_clock::now();
  while (std::chrono::steady_clock::now() - start_time <
         std::chrono::seconds(timeoutSeconds)) {
    if (file.peek() != std::ifstream::traits_type::eof()) {
      // File has bytes, read the PID
      file >> pid;
      file.close();
      // std::remove(pidfile);
      return pid;
    }
    std::this_thread::sleep_for(
      std::chrono::milliseconds(100)); // Wait for 100 milliseconds
    file = std::ifstream(pidfile);
  }

  // Timeout reached, return an error value (e.g., -1)
  return -1;
}

int
forkApp(string cmd, char** envp, string args)
{

  char pidfile[] = "/tmp/pid.XXXXXX"; // Template for temporary file name
  if (isShellScript(cmd)) {
    int fd = mkstemp(pidfile); // Create a temporary file
    close(fd);
    args = string(pidfile) + " " + args;
  }

  int pid = fork();
  if (pid == 0) {
    setsid();

    if (args != "") {
      std::vector<char*> argv;
      std::istringstream iss(args);
      std::vector<std::string> tokens;
      std::string token;
      while (iss >> token) {
        tokens.push_back(token);
      }

      // Prepare array of C-style strings
      argv.push_back(const_cast<char*>(cmd.c_str())); // Command itself
      for (const auto& arg : tokens) {
        argv.push_back(const_cast<char*>(arg.c_str()));
      }
      argv.push_back(nullptr); // Null-terminate the array

      // Execute command with arguments
      execve(cmd.c_str(), argv.data(), envp);
    } else {
      execle(cmd.c_str(), cmd.c_str(), NULL, envp);
    }
    exit(0);
  } else {
    if (isShellScript(cmd)) {
      auto shellPid = getShPID(pidfile);
      return shellPid;
    } else {
      return pid;
    }
  }
}

bool
pidIsRunning(int pid)
{
  return kill(pid, 0) == 0;
}

void
systems::boot(std::shared_ptr<EntityRegistry> registry,
              entt::entity entity,
              char** envp)
{
  auto bootable = registry->try_get<Bootable>(entity);

  if (bootable && bootable->bootOnStartup) {
    if (bootable->pid != std::nullopt && !bootable->killOnExit) {
      // Check if the process exists
      if (pidIsRunning(bootable->pid.value())) {
        return; // Process is already running, no need to fork
      }
    }

    bootable->pid = forkApp(bootable->cmd, envp, bootable->args);
    if (bootable->pid == -1) {
      bootable->pid = nullopt;
    }
  }
}

void
systems::bootAll(std::shared_ptr<EntityRegistry> registry, char** envp)
{
  auto bootables = registry->view<Bootable>();
  for (auto [entity, bootable] : bootables.each()) {
    boot(registry, entity, envp);
  }
}

void
systems::killBootablesOnExit(std::shared_ptr<EntityRegistry> registry)
{
  auto bootables = registry->view<Bootable>();
  for (auto [entity, bootable] : bootables.each()) {
    if (bootable.killOnExit && bootable.pid.has_value()) {
      kill(bootable.pid.value(), SIGTERM);
      bootable.pid = nullopt;
    }
  }
}

std::vector<std::pair<entt::entity, int>>
systems::getAlreadyBooted(std::shared_ptr<EntityRegistry> registry)
{

  std::vector<std::pair<entt::entity, int>> rv;
  auto bootables = registry->view<Bootable>();
  for (auto [entity, bootable] : bootables.each()) {
    if (!bootable.killOnExit && bootable.pid.has_value() &&
        pidIsRunning(bootable.pid.value())) {
      rv.push_back(std::make_pair(entity, bootable.pid.value()));
    }
  }
  return rv;
}

void
systems::resizeBootable(std::shared_ptr<EntityRegistry> registry,
                        entt::entity entity,
                        int width,
                        int height)
{
  auto app = registry->try_get<X11App>(entity);
  auto& bootable = registry->get<Bootable>(entity);
  bootable.resize(width, height);
  if (app) {
    app->resizeMove(width, height, bootable.x, bootable.y);
  }
}

optional<entt::entity>
systems::matchApp(shared_ptr<EntityRegistry> registry, X11App* app)
{
  auto bootableView = registry->view<Bootable>();
  entt::entity entity;
  bool foundEntity = false;

  for (auto [candidateEntity, bootable] : bootableView.each()) {
    if (bootable.name.has_value() && app != NULL &&
        bootable.name.value() == app->getWindowName()) {
      bootable.pid = app->getPID();
      foundEntity = true;
    }
    if (bootable.pid.has_value() && bootable.pid.value() == app->getPID()) {
      foundEntity = true;
    }
    if (foundEntity) {
      app->resize(bootable.getWidth(), bootable.getHeight());
      return candidateEntity;
    }
    if (bootable.name.has_value()) {
      cout << "name: " << bootable.name.value() << endl;
      cout << "appName: " << app->getWindowName() << endl;
    }
  }
  return nullopt;
}
