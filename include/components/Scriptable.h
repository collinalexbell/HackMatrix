#pragma once
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
