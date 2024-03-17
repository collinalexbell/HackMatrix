#pragma once

#include "SQLPersisterImpl.h"
#include "app.h"
#include <string>

struct Bootable {
  std::string cmd;
  std::string args;
  bool killOnExit;
  pid_t pid;
};

class BootablePersister: public SQLPersisterImpl {
public:
  BootablePersister(std::shared_ptr<EntityRegistry> registry)
      : SQLPersisterImpl("Bootable", registry){};
  void createTablesIfNeeded() override;
  void saveAll() override;
  void save(entt::entity) override;
  void loadAll() override;
  void load(entt::entity) override;
  void depersistIfGone(entt::entity) override;
};
