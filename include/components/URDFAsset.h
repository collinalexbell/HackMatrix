#pragma once

#include "SQLPersisterImpl.h"
#include <string>

struct URDFAsset
{
  std::string urdfPath;
};

class URDFAssetPersister : public SQLPersisterImpl
{
public:
  URDFAssetPersister(std::shared_ptr<EntityRegistry> registry)
    : SQLPersisterImpl("URDFAsset", registry){};
  void createTablesIfNeeded() override;
  void saveAll() override;
  void save(entt::entity) override;
  void loadAll() override;
  void load(entt::entity) override;
  void depersistIfGone(entt::entity) override;
};
