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

  void editor(std::filesystem::path filePath) {
    std::string editorCommand =
        "emacsclient -c " + filePath.string(); // Customize with your editor
    auto result = system(editorCommand.c_str());     // Blocks until editor closes
  }

  void editScript(std::shared_ptr<EntityRegistry> registry,
                  entt::entity entity) {

    auto &scriptable = registry->get<Scriptable>(entity);
    std::cout << "fileenum" << scriptable.language << std::endl;
    std::cout << "ext:" << scriptable.getExtension() << std::endl;
    std::thread t([registry, entity, &scriptable]() -> void {

      std::filesystem::path scriptsDir = "./scripts";
      std::string filename =
        "entity" + std::to_string((int)entity) + scriptable.getExtension();
      std::filesystem::path filePath = scriptsDir / filename;

      std::cout << "ext:" << scriptable.getExtension() << std::endl;

      auto fstream = std::ofstream(filePath);
      fstream << scriptable.getScript();
      fstream.close();

      editor(filePath);

      std::ifstream file(filePath);
      if (file.is_open()) {
        std::stringstream buffer;
        buffer << file.rdbuf();
        scriptable.setScript(buffer.str());
      } else {
        // Handle error: Could not open the file
        std::cerr << "Error: Unable to read the edited script" << std::endl;
      }

      if (scriptable.language == PYTHON) {
        std::string runCommand = "python " + filePath.string();
        auto result = system(runCommand.c_str());
      }

      std::filesystem::remove(filePath);

      // This isn't thread safe, be careful how this is done.
      // Maybe make a save<Scriptable>
      //registry->save(entity);
    });
    t.detach();
  }

  void runScript(std::shared_ptr<EntityRegistry> registry,
                 entt::entity entity) {
  }
}
