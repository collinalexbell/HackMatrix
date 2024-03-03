#pragma once
#include <entt.hpp>
#include <memory.h>

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
};

class EntityRegistry;
class SQLPersisterImpl: public SQLPersister {
public:
  std::shared_ptr<EntityRegistry> registry;
  SQLPersisterImpl(std::shared_ptr<EntityRegistry> registry): registry(registry) {}
};

/* easy copy and paste
Constructor(std::shared_ptr<entt::registry> registry):
    SQLPersisterImpl(registry){};
void createTablesIfNeeded() override;
void saveAll() override;
void save(entt::entity) override;
void loadAll() override;
void load(entt::entity) override;


 */
