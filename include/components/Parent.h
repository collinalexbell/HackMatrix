#pragma once
#include "SQLPersisterImpl.h"

struct Parent {
  int parent_entity_id;
};

class ParentPersister: public SQLPersisterImpl {
 public:
  ParentPersister(std::shared_ptr<EntityRegistry> registry)
    : SQLPersisterImpl("Parent", registry){};
  void createTablesIfNeeded() override;
  void saveAll() override;
  void save(entt::entity) override;
  void loadAll() override;
  void load(entt::entity) override;
  void depersistIfGone(entt::entity) override;
};

