#include "components/Parent.h"
#include "SQLiteCpp/Statement.h"
#include <sstream>

void ParentPersister::createTablesIfNeeded() {
  std::stringstream createQueryStream;
  createQueryStream << "CREATE TABLE IF NOT EXISTS " << entityName << "( "
                    << "id INTEGER PRIMARY KEY, "
                    << "entity_id INTEGER, "
                    << "child_id INTEGER, "
                    << "FOREIGN KEY(entity_id) references Entity(id), "
                    << "FOREIGN KEY(child_id) references Entity(id), "
                    << "UNIQUE(entity_id, child_id)"
                    << ")";
  SQLite::Statement query(registry->getDatabase(), createQueryStream.str());
  query.exec();
}

void ParentPersister::saveAll() {
  auto &db = registry->getDatabase();
  auto parentView = registry->view<Persistable, Parent>();

  std::stringstream insertQueryStream;
  insertQueryStream << "INSERT OR REPLACE INTO "
                    << entityName << " "
                    << "(entity_id, child_id) "
                    << "VALUES (?, ?)";

  SQLite::Statement query(db, insertQueryStream.str());

  db.exec("BEGIN TRANSACTION");
  for(auto [entity, persistable, parent]: parentView.each()) {
    for(auto childId: parent.childrenIds) {
      query.bind(1, persistable.entityId);
      query.bind(2, childId);
      query.exec();
      query.reset();
    }
  }
  db.exec("COMMIT");
}

void ParentPersister::save(entt::entity){}
void ParentPersister::loadAll() {
  auto &db = registry->getDatabase();

  std::unordered_map<int, std::vector<int>> parentCache;

  std::stringstream queryStream;
  queryStream << "SELECT entity_id, child_id FROM " << entityName;
  SQLite::Statement query(db, queryStream.str());
  while(query.executeStep()) {
    int entityId = query.getColumn(0).getInt();
    int childId = query.getColumn(1).getInt();
    if(!parentCache.contains(entityId)) {
      parentCache[entityId] = std::vector<int>();
    }
    parentCache[entityId].push_back(childId);
  }

  auto view = registry->view<Persistable>();
  for(auto [entity, persistable]: view.each()) {
    if(parentCache.contains(persistable.entityId)) {
      registry->emplace<Parent>(entity, parentCache[persistable.entityId]);
    }
  }
}
void ParentPersister::load(entt::entity){}
void ParentPersister::depersistIfGone(entt::entity entity) {
  depersistIfGoneTyped<ParentPersister>(entity);
}
