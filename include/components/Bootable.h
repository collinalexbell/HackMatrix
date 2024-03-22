#pragma once

#include "SQLPersisterImpl.h"
#include "app.h"
#include "systems/Boot.h"
#include <string>
#include <glm/glm.hpp>

class Bootable {
  int width;
  int height;

  glm::mat4 heightScaler;
  void recomputeHeightScaler();

public:
  Bootable(std::string cmd, std::string args, bool killOnExit, optional<pid_t> pid,
           bool transparent, optional<std::string> name, bool bootOnStartup = true,
           int width=DEFAULT_WIDTH, int height=DEFAULT_HEIGHT);
  static int DEFAULT_WIDTH;
  static int DEFAULT_HEIGHT;

  // struct members
  std::string cmd;
  std::string args;
  bool killOnExit;
  optional<pid_t> pid;
  bool transparent;
  optional<std::string> name;
  bool bootOnStartup;

  // methods
  int getWidth();
  int getHeight();
  glm::mat4 getHeightScaler();
  friend void systems::resizeBootable(std::shared_ptr<EntityRegistry>, entt::entity,
                                      int width, int height);
  friend class BootablePersister;
};

class BootablePersister : public SQLPersisterImpl {
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
