#include "Config.h"
#include <fstream>

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
