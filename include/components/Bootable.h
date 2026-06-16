#pragma once

#include "SQLPersisterImpl.h"
#include "systems/Boot.h"
#include <optional>
#include <string>
#include <sys/types.h>
#include <glm/glm.hpp>

class Bootable {
  int width;
  int height;

  glm::mat4 heightScaler;
  int defaultXBySize;
  int defaultYBySize;
  void resetDefaultXYBySize();

public:
  Bootable(std::string cmd, std::string args, bool killOnExit,
           std::optional<pid_t> pid, bool transparent,
           std::optional<std::string> name, bool bootOnStartup = true,
           int width=DEFAULT_WIDTH, int height=DEFAULT_HEIGHT,
           std::optional<int> x = std::nullopt,
           std::optional<int> y = std::nullopt);
  static int DEFAULT_WIDTH;
  static int DEFAULT_HEIGHT;

  // struct members
  std::string cmd;
  std::string args;
  bool killOnExit;
  std::optional<pid_t> pid;
  bool transparent;
  std::optional<std::string> name;
  bool bootOnStartup;
  int x;
  int y;

  // methods
  int getWidth();
  int getHeight();
  glm::mat4 getHeightScaler();
  void resize(int width, int height);
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
