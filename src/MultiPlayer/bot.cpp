#include "Multiplayer/Client.h"
#include "entity.h"

int main() {
  auto registry = make_shared<EntityRegistry>();
  auto client = Client(registry);
  // hardcoded, lfg :(
  client.connect("hackrpg.com", 1234);

  // I don't want to connect to the server to test this until
  // the livestream is over
}
