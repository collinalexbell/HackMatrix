#include <filesystem>
#include <fstream>
#include <future>
#include <iostream> // For error reporting
#include <mutex>
#include <sstream>
#include <thread>
#include <cstdlib>
#include <cstring>
#include <sys/wait.h>
#include <unistd.h>

#include "Config.h"
#include "components/Scriptable.h"
#include "systems/Scripts.h"
#include "entity.h"

extern char** environ;

namespace systems {

namespace {

const char*
getEnv(const char* name, char** envp)
{
  if (envp) {
    const size_t nameLen = std::strlen(name);
    for (char** p = envp; *p; ++p) {
      if (std::strncmp(*p, name, nameLen) == 0 && (*p)[nameLen] == '=') {
        return *p + nameLen + 1;
      }
    }
  }
  return std::getenv(name);
}

void
configureWaylandChildEnvironment()
{
  std::string waylandDisplay;
  std::string runtimeDir;

  if (const char* value = getEnv("HACKMATRIX_WAYLAND_DISPLAY", environ)) {
    waylandDisplay = value;
  } else if (const char* value = getEnv("WAYLAND_DISPLAY", environ)) {
    waylandDisplay = value;
  }

  if (const char* value = getEnv("XDG_RUNTIME_DIR", environ)) {
    runtimeDir = value;
  }

  if (!runtimeDir.empty()) {
    std::error_code ec;
    std::filesystem::create_directories(runtimeDir, ec);
  }

  if (!waylandDisplay.empty()) {
    setenv("WAYLAND_DISPLAY", waylandDisplay.c_str(), 1);
  }
  if (!runtimeDir.empty()) {
    setenv("XDG_RUNTIME_DIR", runtimeDir.c_str(), 1);
  }
  if (const char* xwaylandDisplay = std::getenv("HACKMATRIX_DISPLAY")) {
    setenv("DISPLAY", xwaylandDisplay, 1);
  } else {
    unsetenv("DISPLAY");
  }
  setenv("XDG_SESSION_TYPE", "wayland", 1);
  setenv("GDK_BACKEND", "wayland,x11", 1);
  setenv("QT_QPA_PLATFORM", "wayland;xcb", 1);
  setenv("SDL_VIDEODRIVER", "wayland,x11", 1);
  setenv("CLUTTER_BACKEND", "wayland", 1);
  setenv("ELM_DISPLAY", "wl", 1);
  setenv("MOZ_ENABLE_WAYLAND", "1", 1);
  setenv("OZONE_PLATFORM", "wayland", 1);
  setenv("ELECTRON_OZONE_PLATFORM_HINT", "wayland", 1);
}

std::string
shellQuote(const std::string& input)
{
  std::string quoted = "'";
  for (char ch : input) {
    if (ch == '\'') {
      quoted += "'\\''";
    } else {
      quoted += ch;
    }
  }
  quoted += "'";
  return quoted;
}

std::string
readLegacyEditorCommand()
{
  std::ifstream configFile("editor.conf");
  std::string editorCommand;
  if (configFile.is_open()) {
    std::getline(configFile, editorCommand);
  }
  return editorCommand;
}

std::string
buildEditorCommand(const std::filesystem::path& filePath)
{
  if (const char* envCommand = std::getenv("SCRIPTABLE_EDITOR_COMMAND")) {
    std::string command = envCommand;
    if (!command.empty()) {
      return command + " " + shellQuote(filePath.string());
    }
  }

  try {
    auto config = Config::singleton();
    std::string terminalProgram =
      config->get<std::string>("scriptable_terminal_program");
    std::string editorProgram =
      config->get<std::string>("scriptable_editor_program");
    if (!terminalProgram.empty() && !editorProgram.empty()) {
      return terminalProgram + " " + editorProgram + " " +
             shellQuote(filePath.string());
    }
  } catch (...) {
  }

  std::string legacyEditorCommand = readLegacyEditorCommand();
  if (!legacyEditorCommand.empty()) {
    return legacyEditorCommand + " " + shellQuote(filePath.string());
  }

  return "kitty vim " + shellQuote(filePath.string());
}

std::filesystem::path
resolvePythonInterpreter()
{
  if (const char* envPython = std::getenv("SCRIPTABLE_PYTHON")) {
    std::filesystem::path configuredPython = envPython;
    if (!configuredPython.empty() &&
        std::filesystem::exists(configuredPython)) {
      return configuredPython;
    }
  }

  std::filesystem::path venvPython = "./venv/bin/python";
  if (std::filesystem::exists(venvPython)) {
    return venvPython;
  }

  return "python";
}

} // namespace

void
runScript(std::filesystem::path scriptPath, Scriptable& scriptable)
{
  if (scriptable.language == PYTHON) {
    std::filesystem::path pythonInterpreter = resolvePythonInterpreter();
    pid_t pid = fork();
    if (pid == 0) {
      configureWaylandChildEnvironment();
      execl(pythonInterpreter.c_str(),
            pythonInterpreter.c_str(),
            scriptPath.c_str(),
            (char*)nullptr);
      _exit(127);
    }

    if (pid > 0) {
      int status = 0;
      waitpid(pid, &status, 0);
    }
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
  const std::string editorCommand = buildEditorCommand(filePath);

  pid_t pid = fork();
  if (pid == 0) {
    setsid();
    configureWaylandChildEnvironment();
    execl("/bin/sh", "sh", "-c", editorCommand.c_str(), (char*)nullptr);
    _exit(127);
  }

  if (pid > 0) {
    int status = 0;
    waitpid(pid, &status, 0);
  }
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
