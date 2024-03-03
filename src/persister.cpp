#include "persister.h"
#include "entity.h"


void SQLPersisterImpl::depersist(entt::entity entity) {
  auto &persistable = registry->get<Persistable>(entity);
  std:: stringstream queryStream;
  queryStream << "DELETE FROM " << entityName << " WHERE entity_id = ?";
  SQLite::Statement query(registry->getDatabase(), queryStream.str());
  query.bind(1, persistable.entityId);
  try {
    query.exec();
  } catch(...) { /*nothing to delete*/}
}
