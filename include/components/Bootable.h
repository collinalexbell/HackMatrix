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
  int defaultXBySize;
  int defaultYBySize;
  void resetDefaultXYBySize();

public:
  Bootable(std::string cmd, std::string args, bool killOnExit, optional<pid_t> pid,
           bool transparent, optional<std::string> name, bool bootOnStartup = true,
           int width=DEFAULT_WIDTH, int height=DEFAULT_HEIGHT,
           optional<int> x = nullopt, optional<int> y = nullopt);
  static int DEFAULT_WIDTH;
  static int DEFAULT_HEIGHT;
  static constexpr float SCREEN_WIDTH = 1920;
  static constexpr float SCREEN_HEIGHT = 1080;

  // struct members
  std::string cmd;
  std::string args;
  bool killOnExit;
  optional<pid_t> pid;
  bool transparent;
  optional<std::string> name;
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
