#include "components/URDFAsset.h"
#include "SQLiteCpp/Statement.h"
#include "persister.h"
#include <sstream>
#include <unordered_map>

void
URDFAssetPersister::createTablesIfNeeded()
{
  std::stringstream queryStream;
  queryStream << "CREATE TABLE IF NOT EXISTS " << entityName << " ("
              << "entity_id INTEGER PRIMARY KEY, "
              << "urdf_path TEXT, "
              << "FOREIGN KEY(entity_id) REFERENCES Entity(id))";
  registry->getDatabase().exec(queryStream.str());
}

void
URDFAssetPersister::save(entt::entity entity)
{
  if (!registry->all_of<URDFAsset, Persistable>(entity)) {
    return;
  }

  auto& asset = registry->get<URDFAsset>(entity);
  auto& persistable = registry->get<Persistable>(entity);
  auto& db = registry->getDatabase();

  std::stringstream queryStream;
  queryStream << "INSERT OR REPLACE INTO " << entityName
              << " (entity_id, urdf_path) VALUES (?, ?)";
  SQLite::Statement query(db, queryStream.str());
  query.bind(1, persistable.entityId);
  query.bind(2, asset.urdfPath);
  query.exec();
}

void
URDFAssetPersister::saveAll()
{
  auto view = registry->view<Persistable, URDFAsset>();
  auto& db = registry->getDatabase();

  std::stringstream queryStream;
  queryStream << "INSERT OR REPLACE INTO " << entityName
              << " (entity_id, urdf_path) VALUES (?, ?)";
  SQLite::Statement query(db, queryStream.str());

  db.exec("BEGIN TRANSACTION");
  for (auto [entity, persistable, asset] : view.each()) {
    query.bind(1, persistable.entityId);
    query.bind(2, asset.urdfPath);
    query.exec();
    query.reset();
  }
  db.exec("COMMIT");
}

void
URDFAssetPersister::load(entt::entity entity)
{
  if (!registry->all_of<Persistable>(entity)) {
    return;
  }

  auto& persistable = registry->get<Persistable>(entity);
  std::stringstream queryStream;
  queryStream << "SELECT urdf_path FROM " << entityName
              << " WHERE entity_id = ?";
  SQLite::Statement query(registry->getDatabase(), queryStream.str());
  query.bind(1, persistable.entityId);

  if (query.executeStep()) {
    URDFAsset asset;
    asset.urdfPath = query.getColumn(0).getText();
    registry->emplace<URDFAsset>(entity, asset);
  }
}

void
URDFAssetPersister::loadAll()
{
  auto view = registry->view<Persistable>();
  auto& db = registry->getDatabase();

  std::unordered_map<int, std::string> cache;
  std::stringstream queryStream;
  queryStream << "SELECT entity_id, urdf_path FROM " << entityName;
  SQLite::Statement query(db, queryStream.str());
  while (query.executeStep()) {
    int entityId = query.getColumn(0).getInt();
    cache[entityId] = query.getColumn(1).getText();
  }

  for (auto [entity, persistable] : view.each()) {
    auto it = cache.find(persistable.entityId);
    if (it != cache.end()) {
      URDFAsset asset;
      asset.urdfPath = it->second;
      registry->emplace<URDFAsset>(entity, asset);
    }
  }
}

void
URDFAssetPersister::depersistIfGone(entt::entity entity)
{
  depersistIfGoneTyped<URDFAsset>(entity);
}
