#pragma once
#include "Door.h"
#include <entt.hpp>
#include "entity.h"
#include <memory>

#include "SQLPersisterImpl.h"
#include "entity.h"

namespace systems {
  void openDoor(std::shared_ptr<EntityRegistry>, entt::entity);
  void closeDoor(std::shared_ptr<EntityRegistry> , entt::entity);

  class DoorPersister : public SQLPersisterImpl {
    DoorPersister(std::shared_ptr<EntityRegistry> registry)
        : SQLPersisterImpl("Door", registry){};
    void createTablesIfNeeded() override;
    void saveAll() override;
    void save(entt::entity) override;
    void loadAll() override;
    void load(entt::entity) override;
  };
};
