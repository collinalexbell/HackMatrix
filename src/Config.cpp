#include "Config.h"
#include <fstream>

Config::Config()
{
  std::ifstream ifs("config.yaml");
  fkyaml::node config = fkyaml::node::deserialize(ifs);
}

std::shared_ptr<Config> Config::_singleton = nullptr;

std::shared_ptr<Config>
Config::singleton()
{
  if (!_singleton) {
    _singleton = std::make_shared<Config>();
  }
  return _singleton;
}
