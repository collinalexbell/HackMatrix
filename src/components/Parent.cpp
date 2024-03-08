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
                    << "FOREIGN KEY(child_id) references Entity(id)"
                    << ")";
  SQLite::Statement query(registry->getDatabase(), createQueryStream.str());
  query.exec();
}
void ParentPersister::saveAll(){}
void ParentPersister::save(entt::entity){}
void ParentPersister::loadAll(){}
void ParentPersister::load(entt::entity){}
void ParentPersister::depersistIfGone(entt::entity entity) {
  depersistIfGoneTyped<ParentPersister>(entity);
}
