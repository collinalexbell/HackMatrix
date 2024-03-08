#include "components/Scriptable.h"
#include <mutex>

std::string Scriptable::getExtension() {
  switch(language) {
  case CPP:
    return ".cpp";
  case JAVASCRIPT:
    return ".js";
  case PYTHON:
    return ".py";
  default:
    return "";
  }
}

std::string Scriptable::getScript() {
  std::lock_guard<std::mutex> lock(_mutex);
  return script;
}

void Scriptable::setScript(std::string script) {
  std::lock_guard<std::mutex> lock(_mutex);
  this->script = script;
}

Scriptable::Scriptable(std::string script, ScriptLanguage language): script(script), language(language) {}


void ScriptablePersister::createTablesIfNeeded() {
  std::stringstream createQueryStream;
  createQueryStream << "CREATE TABLE IF NOT EXISTS " << entityName << "( "
                    << "entity_id INTEGER PRIMARY KEY, "
                    << "script TEXT, "
                    << "language INTEGER, "
                    << "FOREIGN KEY(entity_id) references Entity(id)"
                    << ")";
  SQLite::Statement query(registry->getDatabase(), createQueryStream.str());
  query.exec();
}

void ScriptablePersister::saveAll() {
  auto &db = registry->getDatabase();
  auto parentView = registry->view<Persistable, Scriptable>();

  std::stringstream insertQueryStream;
  insertQueryStream << "INSERT OR REPLACE INTO " << entityName << " "
                    << "(entity_id, script, language) "
                    << "VALUES (?, ?, ?)";

  SQLite::Statement query(db, insertQueryStream.str());

  db.exec("BEGIN TRANSACTION");
  for (auto [entity, persistable, scriptable] : parentView.each()) {
    query.bind(1, persistable.entityId);
    query.bind(2, scriptable.getScript());
    query.bind(3, scriptable.language);
    query.exec();
    query.reset();
  }
  db.exec("COMMIT");
};
void ScriptablePersister::save(entt::entity entity) {
  auto &db = registry->getDatabase();
  auto [persistable, scriptable] = registry->get<Persistable, Scriptable>(entity);

  std::stringstream insertQueryStream;
  insertQueryStream << "INSERT OR REPLACE INTO " << entityName << " "
                    << "(entity_id, script, language) "
                    << "VALUES (?, ?, ?)";

  SQLite::Statement query(db, insertQueryStream.str());

  query.bind(1, persistable.entityId);
  query.bind(2, scriptable.getScript());
  query.bind(3, scriptable.language);
  query.exec();
};

void ScriptablePersister::loadAll(){
  auto &db = registry->getDatabase();

  struct ScriptableCache {
    std::string script;
    ScriptLanguage language;
  };
  std::unordered_map<int, ScriptableCache> parentCache;

  std::stringstream queryStream;
  queryStream << "SELECT entity_id, script, language FROM " << entityName;
  SQLite::Statement query(db, queryStream.str());
  while (query.executeStep()) {
    int entityId = query.getColumn(0).getInt();
    std::string script = query.getColumn(1).getText();
    int language = query.getColumn(2).getInt();
    parentCache[entityId] = ScriptableCache{script, (ScriptLanguage)language};
  }

  auto view = registry->view<Persistable>();
  for (auto [entity, persistable] : view.each()) {
    if (parentCache.contains(persistable.entityId)) {
      auto scriptableCache = parentCache[persistable.entityId];
      registry->emplace<Scriptable>(entity,
                                    scriptableCache.script,
                                    scriptableCache.language);
    }
  }
};
void ScriptablePersister::load(entt::entity){};
void ScriptablePersister::depersistIfGone(entt::entity entity) {
  depersistIfGoneTyped<Scriptable>(entity);
};
