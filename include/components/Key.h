#pragma once
#include "SQLPersisterImpl.h"
#include "components/RotateMovement.h"
#include <entt.hpp>

enum TurnState {
  TURNED, TURNING, UNTURNED, UNTURNING
};

struct Key {
  int lockable;
  TurnState state;
  RotateMovement turnMovement;
  RotateMovement unturnMovement;
};

class KeyPersister: public SQLPersisterImpl {
 public:
  KeyPersister(std::shared_ptr<EntityRegistry> registry)
    : SQLPersisterImpl("Key", registry){};
  void createTablesIfNeeded() override;
  void saveAll() override;
  void save(entt::entity) override;
  void loadAll() override;
  void load(entt::entity) override;
  void depersistIfGone(entt::entity) override;
};
