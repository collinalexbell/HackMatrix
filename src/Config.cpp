#include "Config.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <stdexcept>

namespace fs = std::filesystem;

namespace {

std::optional<fs::path>
envPath(const char* name)
{
  const char* value = std::getenv(name);
  if (value == nullptr || value[0] == '\0') {
    return std::nullopt;
  }
  return fs::path(value);
}

} // namespace

Config::Config()
{
  configPath = resolveConfigPath();
  std::ifstream ifs(configPath);
  if (!ifs.is_open()) {
    throw std::runtime_error("Unable to open HackMatrix config: " + configPath);
  }
  config = fkyaml::node::deserialize(ifs);
}

std::shared_ptr<Config> Config::_singleton = nullptr;

std::string
Config::resolveConfigPath()
{
  std::vector<fs::path> candidates;

  if (auto explicitPath = envPath("HACKMATRIX_CONFIG_FILE")) {
    return explicitPath->string();
  }

  if (auto configHome = envPath("HACKMATRIX_CONFIG_HOME")) {
    candidates.push_back(*configHome / "config.yaml");
  }

  if (auto xdgConfigHome = envPath("XDG_CONFIG_HOME")) {
    candidates.push_back(*xdgConfigHome / "HackMatrix" / "config.yaml");
  }

  if (auto home = envPath("HOME")) {
    candidates.push_back(*home / ".config" / "HackMatrix" / "config.yaml");
  }

  for (const auto& candidate : candidates) {
    if (fs::exists(candidate)) {
      return candidate.string();
    }
  }

  if (!candidates.empty()) {
    return candidates.front().string();
  }

  return "config.yaml";
}

std::shared_ptr<Config>
Config::singleton()
{
  if (_singleton == nullptr) {
    _singleton = std::make_shared<Config>();
  }
  return _singleton;
}

const std::string&
Config::getConfigPath() const
{
  return configPath;
}

std::vector<std::string>
split_path(const std::string& path)
{
  std::vector<std::string> parts;
  size_t start = 0, end;

  while ((end = path.find('.', start)) != std::string::npos) {
    parts.push_back(path.substr(start, end - start));
    start = end + 1;
  }
  parts.push_back(path.substr(start));
  return parts;
}

std::vector<std::string>
Config::get_keys(const std::string& key_path)
{
  std::vector<std::string> keys;
  auto parts = split_path(key_path);
  fkyaml::node* current = &config;

  // Navigate to the requested node
  for (const auto& part : parts) {
    current = &(*current)[part];
  }

  if(current->is_mapping()) {
  auto keyPairs = current->get_value<std::map<std::string, std::string>>();

    // Collect all keys in the node
    for (const auto& pair : keyPairs) {
        keys.push_back(pair.first);
    }
  }

  return keys;
}
