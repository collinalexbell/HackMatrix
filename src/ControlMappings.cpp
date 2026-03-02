#include "ControlMappings.h"
#include "Config.h"
#include <xkbcommon/xkbcommon.h>



ControlMappings::ControlMappings() {
  auto config = Config::singleton();
  auto mappingKeys = Config::singleton()->get_keys("key_mappings");
  //std::cout << "key_mappings" << mappingKeys[0] << std::endl;
  for(auto it = mappingKeys.begin(); it != mappingKeys.end(); it++) {
    auto fullPath = "key_mappings." + *it;
    auto keyName = config->get<std::string>(fullPath);
    functionNameMap[*it] = keyName;
    xkb_keysym_t keysym =
      xkb_keysym_from_name(keyName.c_str(), XKB_KEYSYM_CASE_INSENSITIVE);
    if(keysym != XKB_KEY_NoSymbol) {
      functionMap[*it] = keysym;
    }
  }
}

int ControlMappings::getKey(std::string fnName) {
  if(functionMap.contains(fnName)) {
    return functionMap[fnName];
  }
  return -1;
}

std::optional<std::string>
ControlMappings::getKeyName(const std::string& fnName) const
{
  auto it = functionNameMap.find(fnName);
  if (it != functionNameMap.end()) {
    return it->second;
  }
  return std::nullopt;
}
