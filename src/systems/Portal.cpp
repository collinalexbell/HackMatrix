#include <systems/Portal.h>

using namespace std;
namespace systems {
 Portal::Portal()  {
 }
 shared_ptr<Portal> Portal::selectApp(entt::entity app) {
  return make_shared<Portal>();
 }
}
