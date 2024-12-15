#include "components/Bootable.h"
#include <optional>
#include <glm/gtx/transform.hpp>
#include "screen.h"

int Bootable::DEFAULT_WIDTH = SCREEN_WIDTH * 0.85;
int Bootable::DEFAULT_HEIGHT = SCREEN_HEIGHT * 0.85;

Bootable::Bootable(std::string cmd, std::string args, bool killOnExit,
                   optional<pid_t> pid, bool transparent,
                   optional<std::string> name, bool bootOnStartup,
                   int width, int height,
                   optional<int> x, optional<int> y)
    : cmd(cmd), args(args), killOnExit(killOnExit), pid(pid),
      transparent(transparent), name(name), bootOnStartup(bootOnStartup),
      width(width), height(height) {

  heightScaler = X11App::recomputeHeightScaler(width, height);
  resetDefaultXYBySize();

  if (x.has_value()) {
    this->x = x.value();
  }
  else {
    this->x = defaultXBySize;
  }

  if (y.has_value()) {
    this->y = y.value();
  } else {
    this->y = defaultYBySize;
  }
}

int Bootable::getWidth() { return width; }
int Bootable::getHeight() { return height; }
glm::mat4 Bootable::getHeightScaler() { return heightScaler; }

void Bootable::resize(int width, int height) {
  this->width = width;
  this->height = height;
  auto oldDefaultXBySize = defaultXBySize;
  auto oldDefaultYBySize = defaultYBySize;
  resetDefaultXYBySize();
  if(x == oldDefaultXBySize || y == oldDefaultYBySize) {
    x = defaultXBySize;
    y = defaultYBySize;
  }
  heightScaler = X11App::recomputeHeightScaler(getWidth(), getHeight());
}

void Bootable::resetDefaultXYBySize() {
  defaultXBySize = (SCREEN_WIDTH - width) / 2;
  defaultYBySize = (SCREEN_HEIGHT - height) / 2;
}



void BootablePersister::createTablesIfNeeded() {
  // even the pid should get saved (used for killOnExit = false)

  SQLite::Database &db = registry->getDatabase();

  std::stringstream create;

  create << "CREATE TABLE IF NOT EXISTS "
         << entityName << " ( "
         << "entity_id INTEGER PRIMARY KEY, "
         << "cmd TEXT,"
         << "args TEXT, "
         << "kill_on_exit INTEGER, "
         << "pid INTEGER, "
         << "transparent INTEGER, "
         << "width INTEGER, "
         << "height INTEGER, "
         << "name TEXT, "
         << "boot_on_startup INTEGER, "
         << "x INTEGER, "
         << "y INTEGER, "
         << "FOREIGN KEY (entity_id) REFERENCES Entity(id)"
         <<")";
  db.exec(create.str());
}
void BootablePersister::saveAll() {
    auto view = registry->view<Persistable, Bootable>();
    SQLite::Database &db = registry->getDatabase();

    stringstream queryStream;
    queryStream << "INSERT OR REPLACE INTO " << entityName << " "
                << "(entity_id, cmd, args, kill_on_exit, pid, "
                << "transparent, width, height, name, boot_on_startup, x, y)"
                << "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";
    SQLite::Statement query(db, queryStream.str());

    db.exec("BEGIN TRANSACTION");
    for (auto [entity, persist, bootable] : view.each()) {
      query.bind(1, persist.entityId);
      query.bind(2, bootable.cmd);
      query.bind(3, bootable.args);
      query.bind(4, bootable.killOnExit ? 1 : 0);
      if(bootable.pid.has_value()) {
        query.bind(5, bootable.pid.value());
      } else {
        query.bind(5, nullptr);
      }
      query.bind(6, bootable.transparent ? 1 : 0);
      query.bind(7, bootable.width);
      query.bind(8, bootable.height);
      if (bootable.name.has_value()) {
        query.bind(9, bootable.name.value());
      } else {
        query.bind(9, nullptr);
      }
      query.bind(10, bootable.bootOnStartup ? 1 : 0);
      query.bind(11, bootable.x);
      query.bind(12, bootable.y);
      query.exec();
      query.reset();
    }
    db.exec("COMMIT");
}

void BootablePersister::save(entt::entity){}
void BootablePersister::loadAll() {
  auto view = registry->view<Persistable>();
  SQLite::Database &db = registry->getDatabase();

  stringstream queryStream;
  queryStream << "SELECT entity_id, cmd, args, kill_on_exit, pid ,"
              << "transparent, width, height, name, boot_on_startup, x, y "
              << "FROM " << entityName;
  SQLite::Statement query(db, queryStream.str());

  while(query.executeStep()) {
    auto entityId = query.getColumn(0).getInt();
    auto cmd = query.getColumn(1).getText();
    auto args = query.getColumn(2).getText();
    bool killOnExit = query.getColumn(3).getInt() == 0 ? false : true;
    optional<int> pid;
    if(query.isColumnNull(4)) {
      pid = nullopt;
    } else {
      pid = query.getColumn(4).getInt();
    }
    bool transparent = query.getColumn(5).getInt() == 0 ? false : true;
    auto width = query.getColumn(6).getInt();
    auto height = query.getColumn(7).getInt();
    optional<std::string> name;
    if(query.isColumnNull(8)) {
      name = nullopt;
    } else {
      name = query.getColumn(8).getText();
    }
    bool bootOnStartup = query.getColumn(9).getInt() == 0 ? false : true;
    optional<int> x, y;
    if(query.isColumnNull(10)) {
      x = nullopt;
    } else {
      x = query.getColumn(10).getInt();
    }
    if(query.isColumnNull(11)) {
      y = nullopt;
    } else {
      y = query.getColumn(11).getInt();
    }
    auto entity = registry->locateEntity(entityId);

    if(entity.has_value()) {
      registry->emplace<Bootable>(entity.value(), cmd, args, killOnExit,
                                  pid, transparent, name, bootOnStartup,
                                  width, height, x, y);
    }
  }
}
void BootablePersister::load(entt::entity){}
void BootablePersister::depersistIfGone(entt::entity entity) {
  depersistIfGoneTyped<Bootable>(entity);
}
