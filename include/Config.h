#pragma once
#include <string>
#include "fkYAML/node.hpp"
#include <memory>

class Config
{
private:
  fkyaml::node config;

public:
  static std::shared_ptr<Config> singleton();
  static std::shared_ptr<Config> _singleton;
  Config();

  template<typename T>
  T get(std::string key)
  {
    return config[key.c_str()].get_value<T>();
  }

  template<typename T>
  T get(const char* key)
  {
    return config[key].get_value<T>();
  }
};
