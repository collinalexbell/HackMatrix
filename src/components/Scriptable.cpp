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
