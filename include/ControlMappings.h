#include <unordered_map>
#include <string>
#include <optional>

// Map configured actions to xkb keysyms.
class ControlMappings {
  std::unordered_map<std::string, int> functionMap;
  std::unordered_map<std::string, std::string> functionNameMap;
 public:
  ControlMappings();
  int getKey(std::string function);
  std::optional<std::string> getKeyName(const std::string& function) const;
};
