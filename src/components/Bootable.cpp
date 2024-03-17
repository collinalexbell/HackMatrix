#include "components/Bootable.h"

void BootablePersister::createTablesIfNeeded() {
  // even the pid should get saved (used for killOnExit = false)
}
void BootablePersister::saveAll() {
  // even the pid should get saved (used for killOnExit = false)
}
void BootablePersister::save(entt::entity){}
void BootablePersister::loadAll(){}
void BootablePersister::load(entt::entity){}
void BootablePersister::depersistIfGone(entt::entity){}
