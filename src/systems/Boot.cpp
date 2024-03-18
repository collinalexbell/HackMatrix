#include "systems/Boot.h"
#include "components/Bootable.h"
#include <signal.h>

int forkApp(string cmd, char **envp, string args) {
  int pid = fork();
  if (pid == 0) {
    setsid();
    if (args != "") {
      execle(cmd.c_str(), cmd.c_str(), args.c_str(), NULL, envp);
    } else {
      execle(cmd.c_str(), cmd.c_str(), NULL, envp);
    }
    exit(0);
  } else {
    return pid;
  }
}

void systems::boot(std::shared_ptr<EntityRegistry> registry,
                   entt::entity entity,
                   char** envp) {
  auto bootable = registry->try_get<Bootable>(entity);
  if(bootable) {
    bootable->pid = forkApp(bootable->cmd, envp, bootable->args);
  }
}

void systems::bootAll(std::shared_ptr<EntityRegistry> registry, char** envp) {
  auto bootables = registry->view<Bootable>();
  for(auto [entity, bootable]: bootables.each()) {
    boot(registry, entity, envp);
  }
}

void systems::killBootablesOnExit(std::shared_ptr<EntityRegistry> registry) {
  auto bootables = registry->view<Bootable>();
  for (auto [entity, bootable] : bootables.each()) {
    if (bootable.killOnExit && bootable.pid.has_value()) {
      kill(bootable.pid.value(), SIGTERM);
      bootable.pid = nullopt;
    }
  }
}
