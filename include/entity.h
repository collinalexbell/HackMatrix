#pragma once

#include <SQLiteCpp/SQLiteCpp.h>
#include <entt.hpp>
#include "persister.h"
#include <vector>
#include <memory>

class EntityRegistry : public entt::registry, public SQLPersister {
  SQLite::Database db;
  std::vector<std::shared_ptr<SQLPersister>> persisters;
public:
  EntityRegistry();
  SQLite::Database &getDatabase();
  void addPersister(std::shared_ptr<SQLPersister>);

  entt::entity createPersistent();
  void createTablesIfNeeded() override;
  void saveAll() override;
  void save(entt::entity) override;
  void loadAll() override;
  void load(entt::entity) override;
};
