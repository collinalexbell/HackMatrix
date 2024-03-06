#pragma once

#include "glm/glm.hpp"
#include "SQLPersisterImpl.h"

enum LockState {
  UNLOCKED, LOCKED
};
struct Lock {
  glm::vec3 position;
  glm::vec3 tolerance;
  LockState state;
};

class LockPersister : public SQLPersisterImpl {
public:
  LockPersister(std::shared_ptr<EntityRegistry> registry)
      : SQLPersisterImpl("Lock", registry){};
  void createTablesIfNeeded() override;
  void saveAll() override;
  void save(entt::entity) override;
  void loadAll() override;
  void load(entt::entity) override;
};
