#include <unordered_map>
#include <string>
#include <GLFW/glfw3.h>
#include <optional>

// Map for direct key mappings (e.g., "W" -> GLFW_KEY_W)
class ControlMappings {
  static std::unordered_map<std::string, int> keyMap;
  std::unordered_map<std::string, int> functionMap;
  std::unordered_map<std::string, std::string> functionNameMap;
 public:
  ControlMappings();
  int getKey(std::string function);
  std::optional<std::string> getKeyName(const std::string& function) const;
};
