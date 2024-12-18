#include "Config.h"
#include <fstream>
#include <map>

Config::Config()
{
  std::ifstream ifs("config.yaml");
  config = fkyaml::node::deserialize(ifs);
}

std::shared_ptr<Config> Config::_singleton = nullptr;

std::shared_ptr<Config>
Config::singleton()
{
  if (_singleton == nullptr) {
    _singleton = std::make_shared<Config>();
  }
  return _singleton;
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
