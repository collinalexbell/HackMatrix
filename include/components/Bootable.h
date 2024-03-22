#pragma once

#include "SQLPersisterImpl.h"
#include "app.h"
#include <string>


struct Bootable {
  static int DEFAULT_WIDTH ;
  static int DEFAULT_HEIGHT;
  std::string cmd;
  std::string args;
  bool killOnExit;
  optional<pid_t> pid;
  bool transparent;
  int width = DEFAULT_WIDTH;
  int height = DEFAULT_HEIGHT;
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
