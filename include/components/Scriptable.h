#pragma once
#include "SQLPersisterImpl.h"
#include "string"
#include <mutex>

enum ScriptLanguage {
  CPP, JAVASCRIPT, PYTHON
};

class Scriptable {
  std::mutex _mutex;
  std::string script;
  bool isDamaged;
 public:
  Scriptable(std::string script, ScriptLanguage language);
  std::string getScript();
  void setScript(std::string);
  ScriptLanguage language;
  std::string getExtension();
  void damage();
  void update();
};

class ScriptablePersister: public SQLPersisterImpl {
 public:
  ScriptablePersister(std::shared_ptr<EntityRegistry> registry):
      SQLPersisterImpl("Entity", registry){};
  void createTablesIfNeeded() override;
  void saveAll() override;
  void save(entt::entity) override;
  void loadAll() override;
  void load(entt::entity) override;
  void depersistIfGone(entt::entity) override;
};

