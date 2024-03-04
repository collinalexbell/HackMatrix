#pragma once
#include <entt.hpp>
#include <memory.h>
#include "persister.h"
#include "entity.h"

class SQLPersisterImpl : public SQLPersister {
public:
  std::string entityName;
  std::shared_ptr<EntityRegistry> registry;
  SQLPersisterImpl(std::string entityName,
                   std::shared_ptr<EntityRegistry> registry)
      : registry(registry), entityName(entityName) {}
  void depersist(entt::entity) override;
  template <typename T> void depersistIfGoneTyped(entt::entity entity) {
    if (!registry->any_of<T>(entity)) {
      depersist(entity);
    }
  }
};
