#pragma once

#include <SQLiteCpp/SQLiteCpp.h>
#include <entt.hpp>
#include "persister.h"
#include <vector>
#include <memory>

class EntityRegistry : public entt::registry {
  SQLite::Database db;
  std::vector<std::shared_ptr<SQLPersister>> persisters;
public:
  EntityRegistry();
  SQLite::Database &getDatabase();
  void addPersister(std::shared_ptr<SQLPersister>);
  void depersist(entt::entity);
  entt::entity createPersistent();
  void createTablesIfNeeded();
  void saveAll();
  void save(entt::entity);
  void loadAll();
  void load(entt::entity);
  template <typename T>
  void removePersistent(entt::entity entity) {
    remove<T>(entity);
    for (auto persister : persisters) {
      persister->depersistIfGone(entity);
    }
  };
};
