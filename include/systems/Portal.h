#include <entt.hpp>
#include <memory.h>
namespace systems {
 class Portal {
  public:
  Portal();
  static std::shared_ptr<Portal> selectApp(entt::entity app);
 };
}
