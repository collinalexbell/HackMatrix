#pragma once
#include <string>
#include "fkYAML/node.hpp"
#include <memory>

std::vector<std::string> split_path(const std::string& path);

  class Config
{
private:
  fkyaml::node config;

public:
  static std::shared_ptr<Config> singleton();
  static std::shared_ptr<Config> _singleton;
  Config();

  template<typename T>
  T get(const std::string& key_path)
  {
    auto parts = split_path(key_path);
    fkyaml::node* current = &config;

    for (size_t i = 0; i < parts.size() - 1; ++i) {
      current = &(*current)[parts[i]];
    }

    return (*current)[parts.back()].get_value<T>();
  }

  std::vector<std::string> get_keys(const std::string& key_path);
};
