#pragma once
#include <memory>
#include <string>
#include <vector>
#include "fkYAML/node.hpp"

std::vector<std::string> split_path(const std::string& path);

class Config
{
private:
  fkyaml::node config;
  std::string configPath;

public:
  static std::shared_ptr<Config> singleton();
  static std::shared_ptr<Config> _singleton;
  Config();
  static std::string resolveConfigPath();
  const std::string& getConfigPath() const;

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
