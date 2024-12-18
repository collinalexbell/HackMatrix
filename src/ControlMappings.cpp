#include "ControlMappings.h"
#include "Config.h"
#include <iostream>

std::unordered_map<std::string, int> ControlMappings::keyMap = {
  { "w", GLFW_KEY_W },
  { "a", GLFW_KEY_A },
  { "s", GLFW_KEY_S },
  { "d", GLFW_KEY_D },
  { "p", GLFW_KEY_P}
};

ControlMappings::ControlMappings() {
  auto config = Config::singleton();
  auto mappingKeys = Config::singleton()->get_keys("key_mappings");
  //std::cout << "key_mappings" << mappingKeys[0] << std::endl;
  for(auto it = mappingKeys.begin(); it != mappingKeys.end(); it++) {
    auto fullPath = "key_mappings." + *it;
    auto key = config->get<std::string>(fullPath);
    if(keyMap.contains(key)) {
      functionMap[*it] = keyMap[key];
    }
  }
}

int ControlMappings::getKey(std::string fnName) {
  if(functionMap.contains(fnName)) {
    return functionMap[fnName];
  }
  return -1;
}
