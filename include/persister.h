#pragma once
#include <entt.hpp>
#include <memory.h>
#include <string>

struct Persistable {
  int64_t entityId;
};

class SQLPersister {
 public:
  virtual void createTablesIfNeeded() = 0;
  virtual void saveAll() = 0;
  virtual void save(entt::entity) = 0;
  virtual void loadAll() = 0;
  virtual void load(entt::entity) = 0;
  virtual void depersist(entt::entity) = 0;
  virtual void depersistIfGone(entt::entity) = 0;
};

/* easy copy and paste
Constructor(std::shared_ptr<EntityRegistry> registry):
    SQLPersisterImpl(registry){};
void createTablesIfNeeded() override;
void saveAll() override;
void save(entt::entity) override;
void loadAll() override;
void load(entt::entity) override;


 */
