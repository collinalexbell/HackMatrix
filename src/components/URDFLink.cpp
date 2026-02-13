#include "components/URDFLink.h"
#include "SQLiteCpp/Statement.h"
#include "persister.h"
#include <sstream>
#include <unordered_map>

void
URDFLinkPersister::createTablesIfNeeded()
{
  auto& db = registry->getDatabase();
  
  std::stringstream queryStream;
  queryStream << "CREATE TABLE IF NOT EXISTS " << entityName << " ("
              << "entity_id INTEGER PRIMARY KEY, "
              << "parent_entity_id INTEGER, "
              << "origin_x REAL, origin_y REAL, origin_z REAL, "
              << "rpy_x REAL, rpy_y REAL, rpy_z REAL, "
              << "axis_x REAL, axis_y REAL, axis_z REAL, "
              << "joint_type INTEGER, joint_value REAL, "
              << "urdf_world_pos_x REAL, urdf_world_pos_y REAL, urdf_world_pos_z REAL, "
              << "urdf_world_rot_x REAL, urdf_world_rot_y REAL, urdf_world_rot_z REAL, "
              << "FOREIGN KEY(entity_id) REFERENCES Entity(id), "
              << "FOREIGN KEY(parent_entity_id) REFERENCES Entity(id))";
  db.exec(queryStream.str());
  
  // Migrate existing tables: add new columns if they don't exist
  // Check if urdf_world_pos_x column exists using PRAGMA table_info
  bool hasNewColumns = false;
  try {
    SQLite::Statement pragma(db, "PRAGMA table_info(" + entityName + ")");
    while (pragma.executeStep()) {
      std::string colName = pragma.getColumn(1).getText();
      if (colName == "urdf_world_pos_x") {
        hasNewColumns = true;
        break;
      }
    }
  } catch (const SQLite::Exception&) {
    // Table might not exist yet, which is fine
  }
  
  if (!hasNewColumns) {
    // Add new columns with default values
    db.exec("ALTER TABLE " + entityName + " ADD COLUMN urdf_world_pos_x REAL DEFAULT 0");
    db.exec("ALTER TABLE " + entityName + " ADD COLUMN urdf_world_pos_y REAL DEFAULT 0");
    db.exec("ALTER TABLE " + entityName + " ADD COLUMN urdf_world_pos_z REAL DEFAULT 0");
    db.exec("ALTER TABLE " + entityName + " ADD COLUMN urdf_world_rot_x REAL DEFAULT 0");
    db.exec("ALTER TABLE " + entityName + " ADD COLUMN urdf_world_rot_y REAL DEFAULT 0");
    db.exec("ALTER TABLE " + entityName + " ADD COLUMN urdf_world_rot_z REAL DEFAULT 0");
  }
}

void
URDFLinkPersister::save(entt::entity entity)
{
  if (!registry->all_of<URDFLink, Persistable>(entity)) {
    return;
  }

  auto& link = registry->get<URDFLink>(entity);
  auto& persistable = registry->get<Persistable>(entity);
  auto& db = registry->getDatabase();

  std::stringstream queryStream;
  queryStream << "INSERT OR REPLACE INTO " << entityName
              << " (entity_id, parent_entity_id, origin_x, origin_y, origin_z, "
              << "rpy_x, rpy_y, rpy_z, axis_x, axis_y, axis_z, "
              << "joint_type, joint_value, "
              << "urdf_world_pos_x, urdf_world_pos_y, urdf_world_pos_z, "
              << "urdf_world_rot_x, urdf_world_rot_y, urdf_world_rot_z)"
              << " VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";
  SQLite::Statement query(db, queryStream.str());

  int parentEntityId = 0;
  if (link.parent != entt::null && registry->valid(link.parent) &&
      registry->all_of<Persistable>(link.parent)) {
    parentEntityId = registry->get<Persistable>(link.parent).entityId;
  }

  query.bind(1, persistable.entityId);
  query.bind(2, parentEntityId);
  query.bind(3, link.originPos.x);
  query.bind(4, link.originPos.y);
  query.bind(5, link.originPos.z);
  query.bind(6, link.originRpy.x);
  query.bind(7, link.originRpy.y);
  query.bind(8, link.originRpy.z);
  query.bind(9, link.axis.x);
  query.bind(10, link.axis.y);
  query.bind(11, link.axis.z);
  query.bind(12, static_cast<int>(link.jointType));
  query.bind(13, link.jointValue);
  query.bind(14, link.urdfWorldPos.x);
  query.bind(15, link.urdfWorldPos.y);
  query.bind(16, link.urdfWorldPos.z);
  query.bind(17, link.urdfWorldRot.x);
  query.bind(18, link.urdfWorldRot.y);
  query.bind(19, link.urdfWorldRot.z);
  query.exec();
}

void
URDFLinkPersister::saveAll()
{
  auto view = registry->view<Persistable, URDFLink>();
  auto& db = registry->getDatabase();

  std::stringstream queryStream;
  queryStream << "INSERT OR REPLACE INTO " << entityName
              << " (entity_id, parent_entity_id, origin_x, origin_y, origin_z, "
              << "rpy_x, rpy_y, rpy_z, axis_x, axis_y, axis_z, "
              << "joint_type, joint_value, "
              << "urdf_world_pos_x, urdf_world_pos_y, urdf_world_pos_z, "
              << "urdf_world_rot_x, urdf_world_rot_y, urdf_world_rot_z)"
              << " VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";
  SQLite::Statement query(db, queryStream.str());

  db.exec("BEGIN TRANSACTION");
  for (auto [entity, persistable, link] : view.each()) {
    int parentEntityId = 0;
    if (link.parent != entt::null && registry->valid(link.parent) &&
        registry->all_of<Persistable>(link.parent)) {
      parentEntityId = registry->get<Persistable>(link.parent).entityId;
    }

    query.bind(1, persistable.entityId);
    query.bind(2, parentEntityId);
    query.bind(3, link.originPos.x);
    query.bind(4, link.originPos.y);
    query.bind(5, link.originPos.z);
    query.bind(6, link.originRpy.x);
    query.bind(7, link.originRpy.y);
    query.bind(8, link.originRpy.z);
    query.bind(9, link.axis.x);
    query.bind(10, link.axis.y);
    query.bind(11, link.axis.z);
    query.bind(12, static_cast<int>(link.jointType));
    query.bind(13, link.jointValue);
    query.bind(14, link.urdfWorldPos.x);
    query.bind(15, link.urdfWorldPos.y);
    query.bind(16, link.urdfWorldPos.z);
    query.bind(17, link.urdfWorldRot.x);
    query.bind(18, link.urdfWorldRot.y);
    query.bind(19, link.urdfWorldRot.z);
    query.exec();
    query.reset();
  }
  db.exec("COMMIT");
}

void
URDFLinkPersister::load(entt::entity entity)
{
  if (!registry->all_of<Persistable>(entity)) {
    return;
  }

  auto& persistable = registry->get<Persistable>(entity);
  std::stringstream queryStream;
  queryStream << "SELECT parent_entity_id, origin_x, origin_y, origin_z, "
              << "rpy_x, rpy_y, rpy_z, axis_x, axis_y, axis_z, "
              << "joint_type, joint_value, "
              << "urdf_world_pos_x, urdf_world_pos_y, urdf_world_pos_z, "
              << "urdf_world_rot_x, urdf_world_rot_y, urdf_world_rot_z "
              << "FROM " << entityName
              << " WHERE entity_id = ?";
  SQLite::Statement query(registry->getDatabase(), queryStream.str());
  query.bind(1, persistable.entityId);

  if (query.executeStep()) {
    int parentId = query.getColumn(0).getInt();
    URDFLink link;
    link.originPos = glm::vec3(query.getColumn(1).getDouble(),
                               query.getColumn(2).getDouble(),
                               query.getColumn(3).getDouble());
    link.originRpy = glm::vec3(query.getColumn(4).getDouble(),
                               query.getColumn(5).getDouble(),
                               query.getColumn(6).getDouble());
    link.axis = glm::vec3(query.getColumn(7).getDouble(),
                          query.getColumn(8).getDouble(),
                          query.getColumn(9).getDouble());
    link.jointType =
      static_cast<URDFLink::JointType>(query.getColumn(10).getInt());
    link.jointValue = query.getColumn(11).getDouble();
    // Load URDF world position/rotation (default to 0 if columns are NULL)
    if (query.getColumn(12).isNull()) {
      link.urdfWorldPos = glm::vec3(0.0f);
    } else {
      link.urdfWorldPos = glm::vec3(query.getColumn(12).getDouble(),
                                    query.getColumn(13).getDouble(),
                                    query.getColumn(14).getDouble());
    }
    if (query.getColumn(15).isNull()) {
      link.urdfWorldRot = glm::vec3(0.0f);
    } else {
      link.urdfWorldRot = glm::vec3(query.getColumn(15).getDouble(),
                                    query.getColumn(16).getDouble(),
                                    query.getColumn(17).getDouble());
    }
    link.dirty = true;

    if (parentId != 0) {
      auto parentEntity = registry->locateEntity(parentId);
      if (parentEntity.has_value()) {
        link.parent = parentEntity.value();
      }
    }

    registry->emplace<URDFLink>(entity, link);
  }
}

void
URDFLinkPersister::loadAll()
{
  auto view = registry->view<Persistable>();
  auto& db = registry->getDatabase();

  struct Cached {
    int parentId;
    URDFLink link;
  };
  std::unordered_map<int, Cached> cache;

  std::stringstream queryStream;
  queryStream << "SELECT entity_id, parent_entity_id, "
              << "origin_x, origin_y, origin_z, "
              << "rpy_x, rpy_y, rpy_z, axis_x, axis_y, axis_z, "
              << "joint_type, joint_value, "
              << "urdf_world_pos_x, urdf_world_pos_y, urdf_world_pos_z, "
              << "urdf_world_rot_x, urdf_world_rot_y, urdf_world_rot_z "
              << "FROM " << entityName;
  SQLite::Statement query(db, queryStream.str());
  while (query.executeStep()) {
    int entityId = query.getColumn(0).getInt();
    int parentId = query.getColumn(1).getInt();
    URDFLink link;
    link.originPos = glm::vec3(query.getColumn(2).getDouble(),
                               query.getColumn(3).getDouble(),
                               query.getColumn(4).getDouble());
    link.originRpy = glm::vec3(query.getColumn(5).getDouble(),
                               query.getColumn(6).getDouble(),
                               query.getColumn(7).getDouble());
    link.axis = glm::vec3(query.getColumn(8).getDouble(),
                          query.getColumn(9).getDouble(),
                          query.getColumn(10).getDouble());
    link.jointType =
      static_cast<URDFLink::JointType>(query.getColumn(11).getInt());
    link.jointValue = query.getColumn(12).getDouble();
    // Load URDF world position/rotation (default to 0 if columns are NULL)
    if (query.getColumn(13).isNull()) {
      link.urdfWorldPos = glm::vec3(0.0f);
    } else {
      link.urdfWorldPos = glm::vec3(query.getColumn(13).getDouble(),
                                    query.getColumn(14).getDouble(),
                                    query.getColumn(15).getDouble());
    }
    if (query.getColumn(16).isNull()) {
      link.urdfWorldRot = glm::vec3(0.0f);
    } else {
      link.urdfWorldRot = glm::vec3(query.getColumn(16).getDouble(),
                                    query.getColumn(17).getDouble(),
                                    query.getColumn(18).getDouble());
    }
    link.dirty = true;
    cache[entityId] = { parentId, link };
  }

  for (auto [entity, persistable] : view.each()) {
    auto it = cache.find(persistable.entityId);
    if (it == cache.end()) {
      continue;
    }
    auto& cached = it->second;
    if (cached.parentId != 0) {
      auto parentEntity = registry->locateEntity(cached.parentId);
      if (parentEntity.has_value()) {
        cached.link.parent = parentEntity.value();
      }
    }
    registry->emplace<URDFLink>(entity, cached.link);
  }
}

void
URDFLinkPersister::depersistIfGone(entt::entity entity)
{
  depersistIfGoneTyped<URDFLink>(entity);
}
