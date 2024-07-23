#include <filesystem>
#include <fstream>
#include <future>
#include <iostream> // For error reporting
#include <mutex>
#include <thread>

#include "components/Scriptable.h"
#include "systems/Scripts.h"
#include "entity.h"

namespace systems {

void
runScript(std::filesystem::path scriptPath, Scriptable& scriptable)
{
  if (scriptable.language == PYTHON) {
    std::string runCommand = "python " + scriptPath.string();
    auto result = system(runCommand.c_str());
  }
}

std::filesystem::path
getScriptPath(entt::entity entity, Scriptable& scriptable)
{
  std::filesystem::path scriptsDir = "./scripts";
  std::string filename =
    "entity" + std::to_string((int)entity) + scriptable.getExtension();
  return scriptsDir / filename;
}

void
editor(std::filesystem::path filePath)
{
  std::ifstream configFile("editor.conf");
  std::string editorCommand;

  if (configFile.is_open()) {
    std::getline(configFile, editorCommand);
    configFile.close();
  } else {
    // Default editor command if config file is not found
    editorCommand = "kitty vim";
  }

  editorCommand += " " + filePath.string();
  auto result = system(editorCommand.c_str()); // Blocks until editor closes
}

void
editScript(std::shared_ptr<EntityRegistry> registry, entt::entity entity)
{

  auto& scriptable = registry->get<Scriptable>(entity);
  std::thread t([registry, entity, &scriptable]() -> void {
    std::cout << "ext:" << scriptable.getExtension() << std::endl;
    auto scriptPath = getScriptPath(entity, scriptable);

    auto fstream = std::ofstream(scriptPath);
    fstream << scriptable.getScript();
    fstream.close();

    editor(scriptPath);

    std::ifstream file(scriptPath);
    if (file.is_open()) {
      std::stringstream buffer;
      buffer << file.rdbuf();
      scriptable.setScript(buffer.str());
    } else {
      // Handle error: Could not open the file
      std::cerr << "Error: Unable to read the edited script" << std::endl;
    }

    runScript(scriptPath, scriptable);

    std::filesystem::remove(scriptPath);

    // This isn't thread safe, be careful how this is done.
    // Maybe make a save<Scriptable>
    // registry->save(entity);
  });
  t.detach();
}
}
