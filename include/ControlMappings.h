#include <unordered_map>
#include <string>
#include <GLFW/glfw3.h>

// Map for direct key mappings (e.g., "W" -> GLFW_KEY_W)
class ControlMappings {
  static std::unordered_map<std::string, int> keyMap;
  std::unordered_map<std::string, int> functionMap;
 public:
  ControlMappings();
  int getKey(std::string function);
};
